# A generic (not host-only) initramfs that can find and mount the squashfs on
# the ISO. Kicksecure's dracut config defaults to host-only, which would bake in
# the build container's hardware and fail to boot anywhere else.
#
# The graphics drivers stay out: with their firmware they are about 120 MB of
# a 170 MB initramfs. The splash draws on the firmware's framebuffer through
# simpledrm until the root file system is up, and the drivers then load from
# it, as they would on an installed system.
#
# The ac driver goes in so systemd's battery check, which runs here and
# powers off on a critically low battery, can see a plugged-in charger.
kver=$(ls -1 /lib/modules | sort -V | tail -1)
dracut --force --no-hostonly --no-hostonly-cmdline \
    --add "dmsquash-live plymouth" \
    --add-drivers "ac" \
    --omit-drivers "amdgpu radeon nouveau i915 xe" \
    --compress zstd \
    /live-initrd.img "$kver"
