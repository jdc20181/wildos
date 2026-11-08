#!/bin/bash
# "Check this USB stick" on the start menu boots with rd.live.check, and
# dmsquash-live runs checkisomd5@.service before it mounts anything. This
# replaces that service's bare progress output with a readable screen and
# waits for the person to read the result.

check() {
    return 0
}

depends() {
    echo dmsquash-live
}

install() {
    inst_multiple checkisomd5 sleep
    inst_simple "$moddir/check-usb.sh" /usr/libexec/wildos/check-usb
    inst_simple "$moddir/checkisomd5-wildos.conf" /etc/systemd/system/checkisomd5@.service.d/wildos.conf
}
