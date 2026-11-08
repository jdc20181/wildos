# Incremental ISO builds: sourced by build.sh when it runs with --incremental.
#
# Two things are kept between runs:
#   /cache/debs/<package>/<hash>/   each Wild OS package's .debs, by a hash of
#                                   its sources and of the builder image, so an
#                                   unchanged package is never built again
#   /work/inc/                      the finished root filesystem of the last
#                                   incremental build, the installer pool, and
#                                   what went into them
#
# A rebuild installs only the packages whose .debs changed into the kept root
# filesystem, re-runs the hooks that have to see the result (the live
# initramfs, the image manifest, the clean-up), and compresses it again. When
# the base of the image changed (package list, hooks, builder image, keys,
# patches, or this build script), or there is no kept root filesystem, it
# builds the root filesystem from scratch instead, and keeps that one.
#
# The kept root filesystem does not pick up updates Debian has published since
# it was made; a normal (full) build does.

INC=$WORK/inc
DEB_CACHE=/cache/debs
# Hooks that would fail or waste time on a finished image: the installer pool
# needs apt's package lists, which the clean-up removed. The pool from the
# last full bootstrap is reused.
INC_SKIP_HOOKS="85-installer-pool.sh"
# zstd level 19 takes several minutes over the whole image; previews trade a
# somewhat larger ISO for a much quicker one.
SQUASH_LEVEL=${WILDOS_SQUASH_LEVEL:-6}

# What the root filesystem is built from, apart from Wild OS's own packages.
inc_base_stamp() {
    (
        cd "$SRC/build"
        find packages.list Dockerfile build.sh run-hooks.sh hooks keys patches -type f -print0 \
            | sort -z | xargs -0 sha256sum
        dpkg-query -W | sha256sum
    ) | sha256sum | cut -c1-16
}

# Builds the Wild OS packages in $WORK/pkgsrc, reusing cached .debs.
inc_build_packages() {
    local pkg name key cached builder
    builder=$(dpkg-query -W | sha256sum | cut -c1-16)
    for pkg in "$WORK"/pkgsrc/*/; do
        [ -e "$pkg/NOT-READY" ] && continue
        name=$(basename "$pkg")
        key=$( (cd "$pkg" && find . -type f -print0 | sort -z | xargs -0 sha256sum; echo "$builder") \
            | sha256sum | cut -c1-16)
        cached=$DEB_CACHE/$name/$key
        if [ -d "$cached" ]; then
            cp "$cached"/*.deb "$WORK/pkgsrc/"
            continue
        fi
        echo "building $name"
        (cd "$pkg" && dpkg-buildpackage -us -uc -b -d -tc >/dev/null)
        rm -rf "$DEB_CACHE/$name" && mkdir -p "$cached"
        for deb in "$WORK"/pkgsrc/*.deb; do
            [ "$(dpkg-deb -f "$deb" Source)" = "$name" ] || [ "$(dpkg-deb -f "$deb" Package)" = "$name" ] \
                && cp "$deb" "$cached/"
        done
    done
}

# One line per .deb in the repo: package name and a hash of its contents.
# The data is hashed rather than the file, because repacked packages get a
# new timestamp in the .deb's own headers every time.
inc_deb_identities() {
    local deb
    for deb in "$WORK"/repo/*.deb; do
        printf '%s %s\n' "$(dpkg-deb -f "$deb" Package)" \
            "$( { dpkg-deb -f "$deb"; dpkg-deb --fsys-tarfile "$deb"; } | sha256sum | cut -c1-16)"
    done | sort
}

# Prints why the kept root filesystem cannot be reused, or nothing if it can.
inc_reuse_blocker() {
    if [ ! -d "$INC/rootfs" ] || [ ! -f "$INC/stamp" ]; then
        echo "no root filesystem kept from an earlier incremental build"
    elif [ "$(cat "$INC/stamp")" != "$(inc_base_stamp)" ]; then
        echo "the package list, hooks, keys, patches, build script or builder image changed"
    elif [ ! -d "$INC/pool" ]; then
        echo "no installer pool kept"
    fi
}

inc_mount() {
    local r=$1
    mount --bind /dev "$r/dev"
    mount --bind /dev/pts "$r/dev/pts"
    mount -t proc proc "$r/proc"
    mount -t sysfs sysfs "$r/sys"
}

inc_umount() {
    local r=$1 m
    for m in sys proc dev/pts dev; do
        mountpoint -q "$r/$m" && umount -l "$r/$m"
    done
    return 0
}

# Brings the kept root filesystem up to date with the repo.
inc_update_rootfs() {
    local r=$INC/rootfs changed=() name hook
    rm -f "$INC/stamp"
    inc_deb_identities > "$WORK/deb-identities"
    while read -r name _; do
        chroot "$r" dpkg-query -W -f='${Status}' "$name" 2>/dev/null | grep -q "ok installed" \
            && changed+=("$name")
    done < <(comm -13 "$INC/deb-identities" "$WORK/deb-identities")

    trap 'inc_umount "$INC/rootfs"' EXIT
    inc_mount "$r"
    mkdir -p "$r/tmp/wildos-inc"
    cp "$WORK"/repo/* "$r/tmp/wildos-inc/"
    # Nothing in a chroot should start services.
    printf '#!/bin/sh\nexit 101\n' > "$r/usr/sbin/policy-rc.d"
    chmod 0755 "$r/usr/sbin/policy-rc.d"

    if [ ${#changed[@]} -gt 0 ]; then
        log "Installing changed packages: ${changed[*]}"
        local debs=() deb
        for deb in "$r"/tmp/wildos-inc/*.deb; do
            name=$(dpkg-deb -f "$deb" Package)
            case " ${changed[*]} " in
                *" $name "*) debs+=("/tmp/wildos-inc/$(basename "$deb")") ;;
            esac
        done
        if ! chroot "$r" env DEBIAN_FRONTEND=noninteractive dpkg -i --force-confnew "${debs[@]}" \
            > "$WORK/inc-dpkg.log" 2>&1 || ! chroot "$r" apt-get check >/dev/null 2>&1; then
            # A changed package needs something the image does not have yet:
            # let apt fetch it, from the image's own sources plus this repo.
            log "Resolving new dependencies"
            echo "deb [trusted=yes] file:/tmp/wildos-inc ./" > "$r/etc/apt/sources.list.d/wildos-inc.list"
            chroot "$r" apt-get update -qq
            chroot "$r" env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq -f \
                -o Dpkg::Options::=--force-confnew >/dev/null
            rm -f "$r/etc/apt/sources.list.d/wildos-inc.list"
        fi
    else
        log "No package changed"
    fi

    local hooks=$WORK/inc-hooks
    rm -rf "$hooks" && mkdir -p "$hooks"
    for hook in "$SRC"/build/hooks/*.sh; do
        case " $INC_SKIP_HOOKS " in *" $(basename "$hook") "*) continue ;; esac
        cp "$hook" "$hooks/"
    done
    bash "$SRC/build/run-hooks.sh" "$r" "$hooks"

    rm -f "$r/usr/sbin/policy-rc.d"
    rm -rf "$r/tmp/wildos-inc" "$hooks"
    inc_umount "$r"
    trap - EXIT
    cp -a "$INC/pool" "$r/live-pool"
}

# After a successful build: remember what the kept root filesystem holds.
inc_save() {
    rm -rf "$INC/pool" && cp -a "$WORK/iso/pool" "$INC/pool"
    inc_deb_identities > "$INC/deb-identities"
    inc_base_stamp > "$INC/stamp"
}
