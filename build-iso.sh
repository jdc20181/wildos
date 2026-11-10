#!/bin/bash
# Builds the Wild OS ISO into ./out using the builder container.
# --incremental reuses the last build (see build/incremental.sh).
# WILDOS_CACHE names the volume or directory kept between builds.
# WILDOS_WORK names the scratch volume: one per worktree, so a build never
# wipes another worktree's work.
#
# --packages builds only the packages this checkout changed from main
# (committed or not), never all of them by accident: a full package build
# takes every core for many minutes, and with several sessions at once it froze
# the PC. WILDOS_ONLY="a b" names them instead; WILDOS_ALL=1 builds everything
# (publishing with --full, and CI).
# Patched Debian rebuilds and the compiler cache are shared by every build on
# the PC (wildos-rebuilds, wildos-ccache), so a new worktree starts warm.
# WILDOS_BUILD_MEMORY caps one build (10g by default): a build that needs more
# fails on its own instead of freezing every other build and VM.
#
# Builds on one PC take turns; WILDOS_BUILD_SLOTS lets that many run at once
# (one by default). WILDOS_PUBLISH=1 marks a publish, which has a slot of its
# own and so never waits behind a development build.
set -euo pipefail
cd "$(dirname "$0")"

if [ "${1:-}" = --packages ] && [ -z "${WILDOS_ONLY+set}" ] && [ "${WILDOS_ALL:-}" != 1 ]; then
    base=$(git merge-base HEAD origin/main 2>/dev/null || git merge-base HEAD main)
    # branding/ is copied into wildos-branding when it builds.
    WILDOS_ONLY=$({ git diff --name-only "$base" -- packages branding; git ls-files --others --exclude-standard -- packages branding; } |
        awk -F/ '$1 == "branding" { print "wildos-branding"; next } NF > 2 { print $2 }' | sort -u | tr '\n' ' ')
    WILDOS_ONLY=${WILDOS_ONLY% }
    export WILDOS_ONLY
    if [ -z "$WILDOS_ONLY" ]; then
        echo "No Wild OS package differs from main here, so there is nothing to build."
        echo "Name packages with WILDOS_ONLY=\"wildos-mail ...\", or set WILDOS_ALL=1 for every package."
        exit 0
    fi
    echo "Building only what changed from main: $WILDOS_ONLY"
fi
if [ "${WILDOS_ALL:-}" = 1 ]; then unset WILDOS_ONLY; fi

# One cache for every build on this PC: the Debian packages Wild OS patches
# (Calamares, KDE libraries) take many minutes to compile and are kept there by
# recipe, so a cache per worktree compiled them again for every agent. A path
# (CI's directory) is still honoured.
cache=${WILDOS_CACHE:-wildos-cache}
case "$cache" in
    /*|./*|[A-Za-z]:*) ;;
    *) [ "$cache" = wildos-cache ] || echo "Using the shared wildos-cache volume, not $cache"; cache=wildos-cache ;;
esac

docker build -t wildos-builder build
mkdir -p out

# How many commits changed each set of patches Wild OS applies to a Debian
# package, for the +wildosN version of the rebuild (wildos_suffix in
# build/build.sh). Counted here because the builder has no git.
patch_counts=
for dir in build/patches/*/; do
    n=$(git rev-list --count HEAD -- "$dir" 2>/dev/null) && patch_counts="$patch_counts $(basename "$dir")=$n"
done

docker run --rm --privileged \
    --memory "${WILDOS_BUILD_MEMORY:-10g}" --memory-swap "${WILDOS_BUILD_MEMORY:-10g}" \
    -e WILDOS_REVISION="${WILDOS_REVISION:-}" \
    -e WILDOS_PATCH_COUNTS="$patch_counts" \
    -e WILDOS_ONLY \
    -e WILDOS_PUBLISH \
    -e WILDOS_BUILD_SLOTS \
    -e WILDOS_CHECKOUT="$PWD" \
    -v "$PWD":/src:ro \
    -v "$PWD/out":/out \
    -v "${WILDOS_WORK:-wildos-work}":/work \
    -v "$cache":/cache \
    -v wildos-rebuilds:/rebuilds \
    -v wildos-ccache:/ccache \
    wildos-builder bash /src/build/build.sh "$@"
