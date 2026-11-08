# Downloads the firmware-specific GRUB packages that the installer adds to the
# target (see wildos-installer's install-bootloader-packages). They cannot be
# in the image itself: grub-pc and grub-efi-amd64 conflict. Only packages not
# already in the image are downloaded, so the pool stays small.
for flavour in efi bios; do
    install -d /live-pool/$flavour/partial
done

apt-get install --yes --download-only --no-install-recommends \
    -o Dir::Cache::archives=/live-pool/efi \
    grub-efi-amd64 grub-efi-amd64-signed shim-signed

apt-get install --yes --download-only --no-install-recommends \
    -o Dir::Cache::archives=/live-pool/bios \
    grub-pc

rm -rf /live-pool/efi/partial /live-pool/bios/partial /live-pool/*/lock
ls /live-pool/efi /live-pool/bios
