#!/bin/bash
# Builds the Wild OS ISO into ./out using the builder container.
# --incremental reuses the last build (see build/incremental.sh).
# WILDOS_CACHE names the volume or directory kept between builds.
set -euo pipefail
cd "$(dirname "$0")"

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
    -e WILDOS_REVISION="${WILDOS_REVISION:-}" \
    -e WILDOS_PATCH_COUNTS="$patch_counts" \
    -e WILDOS_ONLY \
    -v "$PWD":/src:ro \
    -v "$PWD/out":/out \
    -v wildos-work:/work \
    -v "${WILDOS_CACHE:-wildos-cache}":/cache \
    wildos-builder bash /src/build/build.sh "$@"
