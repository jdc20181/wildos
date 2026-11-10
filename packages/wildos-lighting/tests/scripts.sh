#!/bin/sh
# Tests rgb-devices against a fake sysfs, /dev and OpenRGB rules file.
#   scripts.sh LIBEXEC_DIR
set -eu

script=$1/rgb-devices
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
fail=0

check() {
    if [ "$2" = "$3" ]; then
        echo "PASS: $1"
    else
        echo "FAIL: $1: expected '$2', got '$3'"
        fail=1
    fi
}

# Writes the given decimal byte values to a file.
bytes() {
    out=$1
    shift
    : >"$out"
    for b in "$@"; do
        printf "\\$(printf %03o "$b")" >>"$out"
    done
}

usb() {
    d=$root/sys/devices/pci0/usb1/$1
    mkdir -p "$d"
    echo "$2" >"$d/idVendor"
    echo "$3" >"$d/idProduct"
}

# A HID interface with one hidraw node, its descriptor from the remaining
# arguments.
hid() {
    dir=$root/sys/devices/pci0/usb1/$1/$2
    node=$3
    shift 3
    mkdir -p "$dir/hidraw/$node" "$root/sys/class/hidraw" "$root/dev"
    bytes "$dir/report_descriptor" "$@"
    ln -s ../.. "$dir/hidraw/$node/device"
    ln -s "$dir/hidraw/$node" "$root/sys/class/hidraw/$node"
    : >"$root/dev/$node"
    echo "${dir#$root/sys}/hidraw/$node"
}

keyboard="5 1 9 6 161 1 5 7 25 224 41 231 21 0 37 1 117 1 149 8 129 2 192"
vendor="6 0 255 9 1 161 1 21 0 38 255 0 117 8 149 64 9 1 129 2 9 1 145 2 192"
nkro_in_vendor="6 0 255 9 1 161 1 5 7 25 0 41 255 129 2 192"
mouse="5 1 9 2 161 1 9 1 161 0 5 9 25 1 41 3 129 2 192 192"
extended_keyboard_usage="5 12 11 6 0 1 0 161 1 192"

check "no OpenRGB: none" 0 "$(sh "$script" "$root")"

mkdir -p "$root/usr/bin" "$root/usr/lib/udev/rules.d"
printf '#!/bin/sh\n' >"$root/usr/bin/openrgb"
chmod +x "$root/usr/bin/openrgb"
cat >"$root/usr/lib/udev/rules.d/60-openrgb.rules.stock" <<'EOF'
#  Corsair K70 RGB PRO
SUBSYSTEMS=="usb|hidraw", ATTRS{idVendor}=="1b1c", ATTRS{idProduct}=="1b2d", TAG+="uaccess", TAG+="Corsair_K70_RGB_PRO"
#  Whole vendor
SUBSYSTEMS=="usb|hidraw", ATTRS{idVendor}=="1532", TAG+="uaccess", TAG+="Razer"
EOF

check "no devices: none" 0 "$(sh "$script" "$root")"

usb 1-1 1b1c 1b2d
kbd=$(hid 1-1 1-1:1.0/0003:1B1C:1B2D.0001 hidraw0 $keyboard)
light=$(hid 1-1 1-1:1.1/0003:1B1C:1B2D.0002 hidraw1 $vendor)
nkro=$(hid 1-1 1-1:1.2/0003:1B1C:1B2D.0003 hidraw2 $nkro_in_vendor)
usb 1-2 dead beef
other=$(hid 1-2 1-2:1.0/0003:DEAD:BEEF.0004 hidraw3 $vendor)
usb 1-3 1532 0084
razer=$(hid 1-3 1-3:1.0/0003:1532:0084.0005 hidraw4 $mouse)
extended=$(hid 1-3 1-3:1.1/0003:1532:0084.0006 hidraw5 $extended_keyboard_usage)

access() {
    if sh "$script" access "$1" "$root"; then echo granted; else echo refused; fi
}

check "keyboard interface refused" refused "$(access "$kbd")"
check "lighting interface granted" granted "$(access "$light")"
check "keys on a vendor page refused" refused "$(access "$nkro")"
check "keyboard usage with its page in the usage refused" refused "$(access "$extended")"
check "device OpenRGB does not list refused" refused "$(access "$other")"
check "listed vendor, mouse interface granted" granted "$(access "$razer")"
check "path outside hidraw refused" refused "$(access /devices/pci0/usb1/1-1)"
check "path climbing out refused" refused "$(access "/devices/../../etc/hidraw/hidraw0")"

# Two listed devices, the Corsair through three nodes; the unlisted one is
# not counted.
check "counts listed devices once" 2 "$(sh "$script" "$root")"

# Root can open every node, so the permission case is only testable as a
# normal user.
if [ "$(id -u)" != 0 ]; then
    chmod 000 "$root"/dev/hidraw4 "$root"/dev/hidraw5
    check "devices this user cannot open are not counted" 1 "$(sh "$script" "$root")"
fi

rm "$root/usr/bin/openrgb"
check "OpenRGB removed: none" 0 "$(sh "$script" "$root")"
check "OpenRGB removed: refused" refused "$(access "$light")"

exit $fail
