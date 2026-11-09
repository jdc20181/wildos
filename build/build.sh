#!/bin/bash
# Builds the Wild OS live ISO. Runs inside the builder container (see
# Dockerfile); use build.ps1 or build-iso.sh at the repo root to launch it.
#
#   /src    repo checkout, read-only
#   /work   scratch space on a Docker volume (Windows bind mounts cannot hold
#           device nodes or Linux permissions, so the rootfs must live here)
#   /cache  persistent apt archive cache between builds
#   /out    where the finished ISO is written
#
#   build.sh [--incremental | --packages]
#
# --incremental keeps the root filesystem and the built packages for the next
# run, which then only reinstalls what changed (see incremental.sh).
# --packages stops once the packages for the update repository are in
# /out/debs, without making an image.

set -euo pipefail

# bash reads scripts as it executes them, so editing the checkout during a
# build changes the running build. Work from a snapshot instead.
if [ -z "${WILDOS_SNAPSHOT:-}" ]; then
    rm -rf /tmp/src && mkdir -p /tmp/src
    tar -C /src --exclude=./out --exclude=./.git -cf - . | tar -C /tmp/src -xf -
    WILDOS_SNAPSHOT=1 exec bash /tmp/src/build/build.sh "$@"
fi

SRC=/tmp/src
WORK=/work
CACHE=/cache/apt
OUT=/out
SUITE=trixie
VOLID=WILDOS
VERSION=$(sed -n '1s/.*(\(.*\)).*/\1/p' "$SRC/packages/wildos-base/debian/changelog")
ISO_NAME="wildos-${VERSION}-amd64.iso"
KICKSECURE_KEY=/usr/share/keyrings/kicksecure-derivative.asc

log() { printf '\n==> %s\n' "$*"; }

ROOTFS=$WORK/rootfs
SQUASH_LEVEL=19
INCREMENTAL=0
PACKAGES_ONLY=0
[ "${1:-}" = --packages ] && PACKAGES_ONLY=1
if [ "${1:-}" = --incremental ]; then
    INCREMENTAL=1
    source "$SRC/build/incremental.sh"
    ROOTFS=$INC/rootfs
fi

log "Wild OS $VERSION"
rm -rf "$WORK/pkgsrc" "$WORK/repo" "$WORK/rootfs" "$WORK/iso"
mkdir -p "$WORK/pkgsrc" "$WORK/repo" "$WORK/iso" "$CACHE" "$OUT"

log "Building Wild OS packages"
cp -r "$SRC/packages/." "$WORK/pkgsrc/"
# Artwork has one master copy at the repo root; packages that ship it build
# their sizes and formats from it.
rm -rf "$WORK/pkgsrc/wildos-branding/branding"
cp -r "$SRC/branding" "$WORK/pkgsrc/wildos-branding/branding"
# Windows bind mounts report every file as executable. debhelper treats an
# executable debian/install as a script and installs its output instead of its
# contents, which silently produces empty packages.
find "$WORK/pkgsrc" -type f -exec chmod 0644 {} +
chmod 0755 "$WORK"/pkgsrc/*/debian/rules
# A build that feeds the update repository carries a revision (CI passes
# the commit count), so each one is a newer version than the last and apt
# offers it as an update. Local and incremental builds keep the plain
# version, and with it their package cache.
#
# The top entry's date becomes the build time too. debhelper clamps every
# file's mtime to that date, and Qt's QML disk cache (plasmashell keeps one in
# ~/.cache/plasmashell/qmlcache) only recompiles a file whose mtime changed,
# so with a fixed date an update kept running the old compiled taskbar.
# The version suffix of a Debian package Wild OS rebuilds with its own patches
# (build/patches/<name>): +wildosN, where N counts the commits that changed
# those patches, so a rebuild with changed patches is always a newer version.
# With one version for two different builds, apt keeps the build a computer
# has while PackageKit offers the published one as an update for ever.
# build-iso.sh counts them (the builder has no git) and passes
# WILDOS_PATCH_COUNTS="calamares=1 kvantum=2 ..."; without it the count is 1.
wildos_suffix() {
    local entry n=
    for entry in ${WILDOS_PATCH_COUNTS:-}; do
        [ "${entry%%=*}" = "$1" ] && n=${entry#*=}
    done
    echo "+wildos${n:-1}"
}

if [ -n "${WILDOS_REVISION:-}" ]; then
    stamp=$(date -R)
    for changelog in "$WORK"/pkgsrc/*/debian/changelog; do
        sed -i -e "1s/(\([^)]*\))/(\1+r$WILDOS_REVISION)/" \
            -e "0,/^ -- /s/^\( -- .*>\)  .*/\1  $stamp/" "$changelog"
    done
fi
if [ "$INCREMENTAL" = 1 ]; then
    inc_build_packages
else
    for pkg in "$WORK"/pkgsrc/*/; do
        # Work in progress is kept out of the image until it is ready.
        [ -e "$pkg/NOT-READY" ] && continue
        # WILDOS_ONLY, when set, names the packages to build (an update that
        # reuses the rest as published).
        if [ -n "${WILDOS_ONLY+set}" ] && [[ " $WILDOS_ONLY " != *" $(basename "$pkg") "* ]]; then
            continue
        fi
        (cd "$pkg" && DEB_BUILD_OPTIONS="parallel=$(nproc)" dpkg-buildpackage -us -uc -b -d -tc >/dev/null)
    done
fi
find "$WORK/pkgsrc" -maxdepth 1 -name '*.deb' -exec mv {} "$WORK/repo/" \;

# Debian's plymouth declares "Conflicts: console-common", which Kicksecure's
# core layer depends on. The two share no files: the conflict dates from
# console-common's old init script loading keymaps while the boot splash held
# the console, and systemd systems no longer run that script. Republish
# plymouth with only that line removed and a +wildos1 version so apt prefers
# it. When Debian updates plymouth, this picks up the new version on the next
# build, and installed systems get it from the Wild OS update repository.
(
    cd "$WORK"
    # The builder image drops its package lists to stay small.
    apt-get update -qq
    apt-get download -qq plymouth
    deb=$(ls plymouth_*.deb)
    rm -rf plymouth-repack && dpkg-deb -R "$deb" plymouth-repack
    control=plymouth-repack/DEBIAN/control
    sed -i '/^Conflicts: console-common$/d' "$control"
    sed -i 's/^Version: \(.*\)$/Version: \1+wildos1/' "$control"
    if grep -q "console-common" "$control"; then
        echo "ERROR: plymouth's control file changed shape; update the repack" >&2
        exit 1
    fi
    dpkg-deb -b plymouth-repack "$WORK/repo/" >/dev/null
    rm -rf plymouth-repack "$deb"
)
# Calamares with Wild OS's patches from build/patches/calamares, versioned
# +wildos1 so apt prefers it. Building Calamares takes several minutes, so
# the result is kept in the cache and rebuilt only when Debian's version or
# the patches change.
(
    cd "$WORK"
    version=$(apt-cache policy calamares | sed -n 's/^  Candidate: //p')
    stamp=$(cat "$SRC"/build/patches/calamares/*.patch | sha256sum | cut -c1-12)
    cached="/cache/calamares/$version$(wildos_suffix calamares)-$stamp"
    if [ ! -d "$cached" ]; then
        log "Building patched Calamares $version"
        echo "deb-src http://deb.debian.org/debian $SUITE main" > /etc/apt/sources.list.d/wildos-src.list
        apt-get update -qq
        DEBIAN_FRONTEND=noninteractive apt-get build-dep -y -qq calamares >/dev/null
        rm -rf calamares-src && mkdir calamares-src && cd calamares-src
        apt-get source -qq "calamares=$version" >/dev/null
        cd calamares-*/
        mkdir -p debian/patches
        for patch in "$SRC"/build/patches/calamares/*.patch; do
            cp "$patch" debian/patches/
            basename "$patch" >> debian/patches/series
        done
        {
            printf 'calamares (%s%s) %s; urgency=medium\n\n' "$version" "$(wildos_suffix calamares)" "$SUITE"
            printf '  * Wild OS patches: %s\n\n' "$(cd "$SRC/build/patches/calamares" && ls *.patch | tr '\n' ' ')"
            printf ' -- Wild OS Developers <dev@wildos.io>  %s\n\n' "$(date -R)"
            cat debian/changelog
        } > debian/changelog.new
        mv debian/changelog.new debian/changelog
        DEB_BUILD_OPTIONS="nocheck parallel=$(nproc)" dpkg-buildpackage -us -uc -b >/dev/null
        mkdir -p "$cached"
        cp ../calamares_*+wildos*_*.deb "$cached/"
        cd "$WORK" && rm -rf calamares-src
    fi
    cp "$cached"/*.deb "$WORK/repo/"
)
# Kvantum, the engine that draws Qt and KDE apps with the WildOS theme, with
# the patches from build/patches/kvantum (solid primary buttons, rounded
# menus with tight padding, the accent from Settings). Cached like Calamares.
(
    cd "$WORK"
    version=$(apt-cache policy qt-style-kvantum | sed -n 's/^  Candidate: //p')
    if [ -z "$version" ] || [ "$version" = "(none)" ]; then
        apt-get update -qq
        version=$(apt-cache policy qt-style-kvantum | sed -n 's/^  Candidate: //p')
    fi
    stamp=$(cat "$SRC"/build/patches/kvantum/*.patch | sha256sum | cut -c1-12)
    cached="/cache/kvantum/$version$(wildos_suffix kvantum)-$stamp"
    if [ ! -d "$cached" ]; then
        log "Building patched Kvantum $version"
        echo "deb-src http://deb.debian.org/debian $SUITE main" > /etc/apt/sources.list.d/wildos-src.list
        apt-get update -qq
        DEBIAN_FRONTEND=noninteractive apt-get build-dep -y -qq "qt-style-kvantum=$version" >/dev/null
        rm -rf kvantum-src && mkdir kvantum-src && cd kvantum-src
        apt-get source -qq "qt-style-kvantum=$version" >/dev/null
        cd kvantum-*/
        mkdir -p debian/patches
        for patch in "$SRC"/build/patches/kvantum/*.patch; do
            cp "$patch" debian/patches/
            basename "$patch" >> debian/patches/series
        done
        {
            printf 'kvantum (%s%s) %s; urgency=medium\n\n' "$version" "$(wildos_suffix kvantum)" "$SUITE"
            printf '  * Wild OS patches: %s\n\n' "$(cd "$SRC/build/patches/kvantum" && ls *.patch | tr '\n' ' ')"
            printf ' -- Wild OS Developers <dev@wildos.io>  %s\n\n' "$(date -R)"
            cat debian/changelog
        } > debian/changelog.new
        mv debian/changelog.new debian/changelog
        DEB_BUILD_OPTIONS="nocheck parallel=$(nproc)" dpkg-buildpackage -us -uc -b >/dev/null
        mkdir -p "$cached"
        cp ../*+wildos[0-9]*_*.deb "$cached/"
        rm -f "$cached"/*dbgsym*
        cd "$WORK" && rm -rf kvantum-src
    fi
    cp "$cached"/*.deb "$WORK/repo/"
)
# KIO, the file library every KDE app copies and moves files with, with the
# patches from build/patches/kio: a name clash asks in the Wild OS clash
# dialog (wildos-portal) instead of KIO's own. All of KIO's packages are
# republished +wildos1, since they require each other's exact version.
# Cached like Calamares.
(
    cd "$WORK"
    version=$(apt-cache policy libkf6kiocore6 | sed -n 's/^  Candidate: //p')
    if [ -z "$version" ] || [ "$version" = "(none)" ]; then
        apt-get update -qq
        version=$(apt-cache policy libkf6kiocore6 | sed -n 's/^  Candidate: //p')
    fi
    stamp=$(cat "$SRC"/build/patches/kio/*.patch | sha256sum | cut -c1-12)
    cached="/cache/kio/$version$(wildos_suffix kio)-$stamp"
    if [ ! -d "$cached" ]; then
        log "Building patched KIO $version"
        echo "deb-src http://deb.debian.org/debian $SUITE main" > /etc/apt/sources.list.d/wildos-src.list
        apt-get update -qq
        DEBIAN_FRONTEND=noninteractive apt-get build-dep -y -qq "kf6-kio=$version" >/dev/null
        rm -rf kio-src && mkdir kio-src && cd kio-src
        apt-get source -qq "kf6-kio=$version" >/dev/null
        cd kf6-kio-*/
        mkdir -p debian/patches
        for patch in "$SRC"/build/patches/kio/*.patch; do
            cp "$patch" debian/patches/
            basename "$patch" >> debian/patches/series
        done
        {
            printf 'kf6-kio (%s%s) %s; urgency=medium\n\n' "$version" "$(wildos_suffix kio)" "$SUITE"
            printf '  * Wild OS patches: %s\n\n' "$(cd "$SRC/build/patches/kio" && ls *.patch | tr '\n' ' ')"
            printf ' -- Wild OS Developers <dev@wildos.io>  %s\n\n' "$(date -R)"
            cat debian/changelog
        } > debian/changelog.new
        mv debian/changelog.new debian/changelog
        DEB_BUILD_OPTIONS="nocheck nodoc parallel=$(nproc)" dpkg-buildpackage -us -uc -b >/dev/null
        mkdir -p "$cached"
        cp ../*+wildos[0-9]*_*.deb "$cached/"
        rm -f "$cached"/*dbgsym*
        cd "$WORK" && rm -rf kio-src
    fi
    cp "$cached"/*.deb "$WORK/repo/"
)
# KXmlGui, the main window framework of KDE's apps, with the patch from
# build/patches/kxmlgui: windows with a hamburger menu start without their
# menu bar, so every app has a slim header. Republished +wildos1 like KIO.
(
    cd "$WORK"
    version=$(apt-cache policy libkf6xmlgui6 | sed -n 's/^  Candidate: //p')
    if [ -z "$version" ] || [ "$version" = "(none)" ]; then
        apt-get update -qq
        version=$(apt-cache policy libkf6xmlgui6 | sed -n 's/^  Candidate: //p')
    fi
    stamp=$(cat "$SRC"/build/patches/kxmlgui/*.patch | sha256sum | cut -c1-12)
    cached="/cache/kxmlgui/$version$(wildos_suffix kxmlgui)-$stamp"
    if [ ! -d "$cached" ]; then
        log "Building patched KXmlGui $version"
        echo "deb-src http://deb.debian.org/debian $SUITE main" > /etc/apt/sources.list.d/wildos-src.list
        apt-get update -qq
        DEBIAN_FRONTEND=noninteractive apt-get build-dep -y -qq "kf6-kxmlgui=$version" >/dev/null
        rm -rf kxmlgui-src && mkdir kxmlgui-src && cd kxmlgui-src
        apt-get source -qq "kf6-kxmlgui=$version" >/dev/null
        cd kf6-kxmlgui-*/
        mkdir -p debian/patches
        for patch in "$SRC"/build/patches/kxmlgui/*.patch; do
            cp "$patch" debian/patches/
            basename "$patch" >> debian/patches/series
        done
        {
            printf 'kf6-kxmlgui (%s%s) %s; urgency=medium\n\n' "$version" "$(wildos_suffix kxmlgui)" "$SUITE"
            printf '  * Wild OS patches: %s\n\n' "$(cd "$SRC/build/patches/kxmlgui" && ls *.patch | tr '\n' ' ')"
            printf ' -- Wild OS Developers <dev@wildos.io>  %s\n\n' "$(date -R)"
            cat debian/changelog
        } > debian/changelog.new
        mv debian/changelog.new debian/changelog
        DEB_BUILD_OPTIONS="nocheck nodoc parallel=$(nproc)" dpkg-buildpackage -us -uc -b >/dev/null
        mkdir -p "$cached"
        cp ../*+wildos[0-9]*_*.deb "$cached/"
        rm -f "$cached"/*dbgsym*
        cd "$WORK" && rm -rf kxmlgui-src
    fi
    cp "$cached"/*.deb "$WORK/repo/"
)
(cd "$WORK/repo" && apt-ftparchive packages . > Packages)
ls -1 "$WORK/repo"/*.deb
# What the update repository publishes: the Wild
# OS packages and the Debian packages rebuilt with Wild OS changes, without
# debug symbols.
rm -rf "$OUT/debs" && mkdir -p "$OUT/debs"
find "$WORK/repo" -maxdepth 1 \( -name 'wildos-*.deb' -o -name '*+wildos[0-9]*.deb' \) ! -name '*-dbgsym_*' \
    -exec cp {} "$OUT/debs/" \;
if [ "$PACKAGES_ONLY" = 1 ]; then
    rm -rf "$WORK/pkgsrc" "$WORK/repo"
    log "Done: $(ls "$OUT/debs" | wc -l) packages in out/debs"
    exit 0
fi

# security-misc refuses to install unless an account in group sudo already
# exists, to stop admins locking themselves out. The image has no users yet at
# that point, so the build uses Kicksecure's documented skip marker; the cleanup
# hook removes it so later upgrades on real machines keep the safeguard.
#
# The key must sit at the same absolute path outside and inside the chroot,
# because apt resolves signed-by against whichever root it is running in.
install -Dm644 "$SRC/build/keys/kicksecure.asc" "$KICKSECURE_KEY"

# The browsers come from their makers' own repositories. Paths match what the
# installed system uses: brave-keyring owns Brave's key and Brave's package
# writes its own apt source; wildos-base ships Mullvad's source and key.
BRAVE_KEY=/usr/share/keyrings/brave-browser-archive-keyring.gpg
MULLVAD_KEY=/usr/share/keyrings/mullvad-keyring.asc
install -Dm644 "$SRC/build/keys/brave-browser.gpg" "$BRAVE_KEY"
install -Dm644 "$SRC/build/keys/mullvad.asc" "$MULLVAD_KEY"

DEBIAN_COMPONENTS="main contrib non-free non-free-firmware"
PACKAGES=$(grep -vE '^\s*(#|$)' "$SRC/build/packages.list" | tr '\n' ',' | sed 's/,$//')
# The image installs no Recommends, but those of Wild OS's own packages are
# the default apps and the helpers each package was written to use. Taken
# from their control files so a new Recommends reaches the image without a
# second list to keep in step. Windows mode's virtual PC is the exception:
# setting Windows mode up installs it. It stays a Recommends so PCs that had
# it before the split keep it on upgrade, since apt adds a Recommends only
# when it is new to the package.
OWN_RECOMMENDS=$(awk '/^Recommends:/ { on = 1; sub(/^Recommends:/, ""); print; next }
                      on && /^[ \t]/ { print; next } { on = 0 }' "$SRC"/packages/*/debian/control |
    tr ',' '\n' | sed 's/\$[{][^}]*[}]//g; s/[[:space:]]//g; s/(.*//; s/|.*//' | grep -v '^$' |
    grep -vx wildos-winmode-runtime | sort -u | paste -sd,)
PACKAGES="$PACKAGES,$OWN_RECOMMENDS"

REUSE_BLOCKER=""
if [ "$INCREMENTAL" = 1 ]; then
    REUSE_BLOCKER=$(inc_reuse_blocker)
fi
if [ "$INCREMENTAL" = 1 ] && [ -z "$REUSE_BLOCKER" ]; then
    log "Updating the kept root filesystem"
    inc_update_rootfs
else
if [ "$INCREMENTAL" = 1 ]; then
    log "Full rebuild of the root filesystem: $REUSE_BLOCKER"
    rm -f "$INC/stamp"
fi
rm -rf "$ROOTFS" && mkdir -p "$(dirname "$ROOTFS")"
log "Bootstrapping root filesystem"
# adduser goes in straight after the essential packages: security-misc
# creates groups in its preinst, which runs before apt has configured
# adduser unless something else pulled adduser in earlier.
mmdebstrap \
    --mode=root \
    --variant=apt \
    --architectures=amd64 \
    --include="$PACKAGES" \
    --aptopt='APT::Install-Recommends "false"' \
    --skip=download/empty \
    --skip=essential/unlink \
    --setup-hook='mkdir -p "$1/var/cache/apt/archives" "$1/usr/share/keyrings" "$1/var/lib/security-misc" "$1/etc/dpkg/dpkg.cfg.d" "$1/etc/apt/preferences.d"' \
    --setup-hook="copy-in $SRC/packages/wildos-base/files/apt/wildos-freerdp.pref /etc/apt/preferences.d/" \
    --setup-hook="copy-in $SRC/build/90-wildos-slim /etc/dpkg/dpkg.cfg.d/" \
    --essential-hook='APT_CONFIG="$MMDEBSTRAP_APT_CONFIG" apt-get install --yes -qq adduser >/dev/null' \
    --setup-hook='touch "$1/var/lib/security-misc/skip_install_check"' \
    --setup-hook="sync-in $CACHE /var/cache/apt/archives/" \
    --setup-hook="copy-in $KICKSECURE_KEY $BRAVE_KEY $MULLVAD_KEY /usr/share/keyrings/" \
    --customize-hook="sync-out /var/cache/apt/archives $CACHE" \
    --customize-hook="bash $SRC/build/run-hooks.sh \"\$1\" $SRC/build/hooks" \
    "$SUITE" "$ROOTFS" \
    "deb http://deb.debian.org/debian $SUITE $DEBIAN_COMPONENTS" \
    "deb http://deb.debian.org/debian $SUITE-updates $DEBIAN_COMPONENTS" \
    "deb http://deb.debian.org/debian $SUITE-backports $DEBIAN_COMPONENTS" \
    "deb http://security.debian.org/debian-security $SUITE-security $DEBIAN_COMPONENTS" \
    "deb [signed-by=$KICKSECURE_KEY] https://deb.kicksecure.com $SUITE main contrib non-free" \
    "deb [signed-by=$BRAVE_KEY arch=amd64] https://brave-browser-apt-release.s3.brave.com/ stable main" \
    "deb [signed-by=$MULLVAD_KEY arch=amd64] https://repository.mullvad.net/deb/stable stable main" \
    "deb [trusted=yes] copy://$WORK/repo ./"
fi

# systemd refuses to switch root into a tree without os-release, which surfaces
# only at boot as an unhelpful emergency shell. Catch it here instead.
if [ ! -s "$ROOTFS/etc/os-release" ]; then
    echo "ERROR: rootfs has no os-release; wildos-base did not install correctly" >&2
    exit 1
fi

# The installer adds exactly one GRUB integration package to the target. If the
# image already has one, the pool's version would conflict with it.
for pkg in grub-pc grub-efi-amd64; do
    if chroot "$ROOTFS" dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "ok installed"; then
        echo "ERROR: $pkg is installed in the image; it must only come from the installer pool" >&2
        exit 1
    fi
done

log "Assembling ISO tree"
KVER=$(basename "$(ls -1d "$ROOTFS"/lib/modules/* | sort -V | tail -1)")
mkdir -p "$WORK/iso/live" "$WORK/iso/LiveOS" "$WORK/iso/boot/grub"
cp "$ROOTFS/boot/vmlinuz-$KVER" "$WORK/iso/live/vmlinuz"
# The live initramfs is generated by a hook outside /boot so the installed
# system never inherits it.
mv "$ROOTFS/live-initrd.img" "$WORK/iso/live/initrd.img"
# GRUB packages for the installer; must sit outside the squashfs or the
# installed system would carry a copy of both.
mv "$ROOTFS/live-pool" "$WORK/iso/pool"
LANGUAGES="$SRC/packages/wildos-live/files/languages"
# One entry per language: picking it shows the main menu again with that
# language passed to the live system.
grep -v '^#' "$LANGUAGES" | awk -F '\t' '{
    printf "    menuentry \"%s\" --class wildos-lang-%s {\n        set wildos_lang=%s\n        export wildos_lang\n        configfile /boot/grub/grub.cfg\n    }\n", $3, $1, $1
}' > "$WORK/grub-languages.cfg"
sed "s/@VOLID@/$VOLID/g; s/@VERSION@/$VERSION/g" "$SRC/build/iso/grub.cfg" \
    | sed -e "/^@LANGUAGES@\$/{r $WORK/grub-languages.cfg" -e 'd}' > "$WORK/iso/boot/grub/grub.cfg"
bash "$SRC/packages/wildos-branding/tools/grub-theme.sh" usb "$WORK/iso/boot/grub/themes/wildos" \
    "$SRC/branding/logo/wild-os-icon.svg" "$VERSION" "$LANGUAGES"

log "Compressing root filesystem"
mksquashfs "$ROOTFS" "$WORK/iso/LiveOS/squashfs.img" \
    -noappend -comp zstd -Xcompression-level "$SQUASH_LEVEL" -b 1M \
    -wildcards -e 'proc/*' 'sys/*' 'dev/*' 'run/*' 'tmp/*'

log "Writing $ISO_NAME"
bash "$SRC/build/iso/make-iso.sh" "$WORK/iso" "$OUT/$ISO_NAME" "$VOLID"
(cd "$OUT" && sha256sum "$ISO_NAME" > "$ISO_NAME.sha256")

[ "$INCREMENTAL" = 1 ] && inc_save

# The rootfs and ISO tree are about 10 GB. A normal build makes them from
# scratch every time and keeps only the apt cache; an incremental one keeps
# its rootfs in $WORK/inc instead.
rm -rf "$WORK/rootfs" "$WORK/iso" "$WORK/pkgsrc" "$WORK/repo"

log "Done: out/$ISO_NAME ($(du -h "$OUT/$ISO_NAME" | cut -f1))"
