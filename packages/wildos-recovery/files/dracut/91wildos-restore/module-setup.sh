#!/bin/bash
# Going back to a restore point at start-up (ext4 systems), and starting a
# Btrfs restore point from the start-up menu on a layer in memory.

check() {
    [ -x /usr/libexec/wildos/restore-points ] || return 1
    return 0
}

depends() {
    return 0
}

install() {
    inst_multiple chroot mount umount findmnt sync reboot mkdir sed head
    inst_hook pre-pivot 50 "$moddir/wildos-restore.sh"
}

installkernel() {
    hostonly='' instmods overlay
}
