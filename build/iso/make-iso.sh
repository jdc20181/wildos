#!/bin/bash
# Writes the bootable ISO from an assembled tree.
#
#   make-iso.sh <tree> <output.iso> <volume label>
#
# UEFI boots through Debian's signed shim and GRUB, so the stick starts with
# Secure Boot on, as most laptops ship. The signed GRUB is a single image with
# its modules built in (it may not load unsigned ones); it finds the stick by
# the /.disk/info file and reads /boot/grub/grub.cfg, the same layout Debian's
# own live images use.
#
# Legacy BIOS gets GRUB's El Torito image, which starts the same grub.cfg and
# menu from /boot/grub. The ISO is hybrid, so this works from a CD or a stick.
#
# The tree must contain pool/efi/ with the shim-signed and
# grub-efi-amd64-signed packages (85-installer-pool.sh puts them there).

set -euo pipefail

tree=$1
out=$2
volid=$3
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

for pkg in shim-signed grub-efi-amd64-signed shim-helpers-amd64-signed; do
    dpkg-deb -x "$(ls "$tree"/pool/efi/${pkg}_*.deb)" "$work/debs"
done
shim=$work/debs/usr/lib/shim/shimx64.efi.signed
mok=$work/debs/usr/lib/shim/mmx64.efi.signed
grub=$work/debs/usr/lib/grub/x86_64-efi-signed/gcdx64.efi.signed

mkdir -p "$tree/.disk"
echo "Wild OS live" > "$tree/.disk/info"

# The EFI system partition image the firmware boots from.
efi_dir=$tree/boot/grub
mkdir -p "$efi_dir"
efi_img=$efi_dir/efi.img
rm -f "$efi_img"
# Shim, the CD GRUB and the key manager come to about 5 MB.
mkfs.msdos -C "$efi_img" 16384 >/dev/null
mmd -i "$efi_img" ::/EFI ::/EFI/boot
mcopy -i "$efi_img" "$shim" ::/EFI/boot/bootx64.efi
mcopy -i "$efi_img" "$grub" ::/EFI/boot/grubx64.efi
mcopy -i "$efi_img" "$mok" ::/EFI/boot/mmx64.efi

# The same files as plain ISO files: Windows tools such as Rufus, in their
# default mode, copy the ISO's files to a FAT stick, which then boots from
# /EFI/boot on that copy rather than from efi.img.
mkdir -p "$tree/EFI/boot"
cp "$shim" "$tree/EFI/boot/bootx64.efi"
cp "$grub" "$tree/EFI/boot/grubx64.efi"
cp "$mok" "$tree/EFI/boot/mmx64.efi"

# Legacy BIOS: a GRUB core for CD booting plus its modules, and the console
# font UEFI GRUB carries inside its signed image.
mkdir -p "$tree/boot/grub/i386-pc" "$tree/boot/grub/fonts"
cp /usr/lib/grub/i386-pc/*.mod /usr/lib/grub/i386-pc/*.lst "$tree/boot/grub/i386-pc/"
cp /usr/share/grub/unicode.pf2 "$tree/boot/grub/fonts/"
grub-mkimage -O i386-pc-eltorito -p /boot/grub -o "$tree/boot/grub/i386-pc/eltorito.img" \
    biosdisk iso9660 normal search search_label configfile gfxterm gfxmenu \
    all_video png font echo test halt

# -partition_offset 16 gives the stick's GPT a first partition holding a
# mountable copy of the file system, as Ubuntu's images do. Without it a
# stick booted by UEFI starts GRUB from the appended EFI partition, GRUB
# finds no file system with /.disk/info, and stops at "file /boot/ not
# found". A CD, and BIOS from a stick, worked either way.
xorriso -as mkisofs -o "$out" -V "$volid" -iso-level 3 -r -J \
    -partition_offset 16 \
    -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table \
    --grub2-boot-info --grub2-mbr /usr/lib/grub/i386-pc/boot_hybrid.img \
    -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot \
    -append_partition 2 0xef "$efi_img" -appended_part_as_gpt \
    "$tree" 2>&1 | grep -vE "^xorriso : (UPDATE|NOTE)" || true

test -s "$out"

# "Check this USB stick" compares the stick with this checksum (dracut's
# rd.live.check runs checkisomd5). It covers the ISO 9660 volume, which holds
# every file the stick boots from, including a copy of the EFI image.
implantisomd5 --force "$out" >/dev/null
