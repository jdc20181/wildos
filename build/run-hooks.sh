#!/bin/bash
# Runs every hook in a directory, in name order, inside the chroot. Called by
# mmdebstrap as a customize hook, which has already mounted /proc, /sys and
# /dev in the target.

set -euo pipefail

rootfs=$1
hooks=$2

for hook in "$hooks"/*.sh; do
    printf -- '--> hook %s\n' "$(basename "$hook")"
    chroot "$rootfs" /usr/bin/env -i \
        PATH=/usr/sbin:/usr/bin:/sbin:/bin \
        DEBIAN_FRONTEND=noninteractive \
        LANG=C.UTF-8 \
        /bin/bash -euo pipefail -s < "$hook"
done
