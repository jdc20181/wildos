# Restore points, start-up recovery and reset

The design behind section 17 of the boards: Settings > System > Recovery
(17.1), going back (17.2), start-up recovery (17.3, 17.4) and Reset Wild OS
(17.5), plus the "Go back" link on Settings > Updates (6.3).

Checked on Debian 13 before building:

| Part | Version in trixie | What it means here |
| --- | --- | --- |
| GRUB | 2.12-9 | Reads LUKS2 only with a PBKDF2 key slot; Wild OS uses Argon2id |
| Calamares | 3.3.14 (ours, patched) | `mount.conf` `btrfsSubvolumes` and `btrfsSwapSubvol` work; the fstab module writes every subvolume; a Btrfs swap file goes in `/swap/swapfile` with `chattr +C` |
| dracut | 106 | The live initramfs on the ISO already has `systemd-cryptsetup`, `dmsquash-live` and the Wild OS splash |
| btrfs-progs | 6.14 | |
| snapper | 0.10.6 | Not used, see below |
| grub-btrfs | not in trixie | Our own menu script instead |
| rsync | 3.5.0 | `--link-dest` restore points on ext4 |

## Two kinds of restore point, one interface

`/usr/libexec/wildos/restore-points` (package `wildos-recovery`) is the only
thing that makes, lists, prunes or applies restore points. It picks the
backend from the root filesystem:

- **Btrfs** (new installs): a read-only snapshot of the `@` subvolume.
  Instant, and only what changes afterwards costs space.
- **ext4** (every install made before this change): a file copy with rsync,
  hard-linked to the previous restore point (`--link-dest`), as Timeshift's
  rsync mode does. Unchanged files cost nothing after the first copy.

Settings, the apt hooks and start-up recovery all call the same tool, so
both layouts look and behave the same.

Each restore point has an `info.json`: id, label ("Before the build 331
update", "Before installing the graphics driver"), time, kind (update,
driver, manual, undo), the kernel versions in it, and a summary of what
changed afterwards. The summary is built from `/var/log/dpkg.log` between
the snapshot and the end of the apt run: system files, drivers (NVIDIA,
firmware, kernel), and apps by name. An index of every point,
`/var/lib/wildos/restore-points/index.json`, is world-readable so Settings
can list them without a password; the restore points themselves are
root-only.

### Why not snapper

snapper's space limit (`SPACE_LIMIT`) needs Btrfs quota groups. qgroups make
deleting snapshots and balancing much slower and have a history of
accounting bugs, and snapper does nothing for ext4. The tool measures space
itself (below) and keeps both layouts on one code path.

## New installs: the Btrfs layout

Still GPT, 512 MB EFI, 1 GB `/boot` ext4 unencrypted, then one LUKS2
partition (Argon2id, unchanged). Inside it, Btrfs (`compress=zstd:1`,
`noatime`, `discard=async` on SSDs) with these subvolumes:

| Subvolume | Mounted at | Goes back with the system? | Why |
| --- | --- | --- | --- |
| `@` | `/` | yes | the system |
| `@home` | `/home` | no | personal files |
| `@snapshots` | `/.snapshots` | no | the restore points themselves |
| `@log` | `/var/log` | no | logs of what went wrong survive going back |
| `@cache` | `/var/cache` | no | apt's downloads; nothing worth keeping in a restore point |
| `@tmp` | `/var/tmp` | no | temporary |
| `@winmode` | `/var/lib/wildos/winmode` | no, copy-on-write off | Windows' disk and state. Going back must never roll back Windows |
| `@libvirt` | `/var/lib/libvirt` | no, copy-on-write off | Windows mode's firmware variables, TPM state and saved memory. A TPM rolled back without its disk can make Windows ask for its BitLocker recovery key |
| `@reset` | `/var/lib/wildos/reset` | no | the reset copy (2 GB). Inside `@` every replaced copy would stay pinned by every restore point |
| `@swap` | `/swap` | no, copy-on-write off | Calamares' own swap file subvolume (a swap file cannot live in a snapshotted subvolume) |

Copy-on-write is turned off with `chattr +C` on the empty `@winmode` and
`@libvirt` directories before anything is copied in, by a shellprocess step
right after `mount` (copy-on-write images fragment badly and double the
writes). Calamares does the same for the swap file itself; setting it on
`@swap` as well makes Calamares' own `chattr +C +m` fail and stops the
install (found in the VM).

Installer changes: `partition.conf` (Btrfs for the root, the only
filesystem offered on the automatic choices), `mount.conf` (subvolumes and
mount options; in Calamares 3.3 the mount options moved there from
`fstab.conf`), `settings.conf` (two shellprocess steps: copy-on-write off
after `mount`, and the reset copy after `unpackfs`). The encryption page,
the recovery key, the disk password at start-up and `finalize-target` are
untouched: the root is still `/dev/mapper/luks-*` inside the same LUKS2
partition, unlocked by dracut. Manual partitioning still offers every
filesystem; restore points then use whichever backend fits.

`/etc/fstab` gets one line per subvolume (`subvol=@`, `subvol=@home`, ...).
The kernel command line gets `rootflags=subvol=@` from GRUB's own probe
(`grub-mkconfig` adds it for a Btrfs root). Wild OS never changes the
default subvolume, so "which system starts" is always "whatever is called
`@`".

## Point 1: kernels and /boot

`/boot` is outside the encrypted disk, so snapshots of `@` never include the
kernel or the initramfs. Putting `/boot` inside Btrfs would need GRUB to
unlock the disk, and GRUB 2.12 cannot read an Argon2id key slot. Switching to
PBKDF2 would weaken the disk password against guessing, and GRUB's own
unlock is slow and asks for the password a second time. So `/boot` stays
where it is, and every restore point carries its own boot files:

- **Each restore point keeps a copy of `/boot`** (kernels, initramfs files,
  `grub.cfg`; not the EFI partition, `grubenv` or GRUB's own modules,
  which must match the boot loader in the EFI partition). On Btrfs it is
  `/.snapshots/<id>/boot`, on ext4 it is part of the file copy. Copies are
  hard-linked to the previous point's, so a kernel costs space once however
  many points share it (typically 60 to 120 MB per kernel and initramfs).
- **Going back puts `/boot` back as well**, so the system that starts is
  always the kernel and initramfs that belonged to it. No newer kernel ever
  meets an older system, and no initramfs built for a newer system (for
  example with NVIDIA's driver in it) starts an older one.
- **Starting a restore point from the GRUB menu** (Btrfs only, under
  "Restore points" in the menu Esc shows) needs that point's kernel on the
  unencrypted `/boot`. A kernel hook (`/etc/kernel/postrm.d`) keeps the
  kernel and initramfs of any version a restore point still uses in
  `/boot/wildos-restore/<version>/` when apt removes it, and pruning deletes
  them once no point needs them. The menu pairs each point with its own
  kernel; a point whose kernel is gone gets no menu entry (going back still
  works, from its own copy). These starts run on a throwaway layer in
  memory over the read-only snapshot, so trying one changes nothing.

## Point 2: what is never snapshotted, and the 20 GB cap

On Btrfs, the separate subvolumes above are simply outside `@`. On ext4 the
same paths are excluded from the file copy and from the restore, so they
stay exactly as they are when going back:

- Windows mode: `/var/lib/wildos/winmode`, `/var/lib/libvirt`,
  `/etc/libvirt` (the VM definition) and `/etc/wildos/winmode.json`
- `/var/log`, `/var/cache`, `/var/tmp`, `/tmp`
- `/home`, the reset copy and the restore point store itself
- every swap file, wherever the system's fstab and `/proc/swaps` put it
  (`/swapfile`, `/var/swapfile` on installs before this change,
  `/swap/swapfile` on Btrfs). A swap file is never copied, and never written
  while in use
- `/proc`, `/sys`, `/dev`, `/run`, `/mnt`, `/media`, `/lost+found`

**Space.** Without qgroups, the tool measures what restore points cost
itself, after each one is made (in the background, at idle I/O priority,
never in the way of the update):

- Btrfs: `btrfs filesystem du -s` over every snapshot reports, per
  snapshot, the data no other file shares (Exclusive). The cost of the
  restore points is the exclusive data of all of them plus their boot
  copies. Data that two old snapshots still share, but the running system
  no longer has, is in neither's exclusive figure, so on Btrfs the number
  can run low. That is the limit of measuring without qgroups; in
  practice most of what an update replaces belongs to one snapshot only.
- ext4: `du -s` over the whole store counts each hard-linked file once,
  which is exact.

Pruning keeps the newest 5 and deletes the oldest until the total is under
20 GB. A point that is the only way to undo a go-back done in the last
hour is never pruned in that hour. Settings > Recovery > Change sets 3, 5
or 10 points and 10, 20 or 40 GB, or turns restore points off.

## Point 3: going back, and where it runs

**From Settings, Btrfs.** No offline step is needed. Online, under a
lock: snapshot the restore point as a new writable `@new`; carry the
state that stays across (below); copy the point's `/boot` back; rename `@`
to a new restore point ("Before going back", the undo point) and `@new`
to `@`. The running system keeps using the old subvolume until the restart;
the next start mounts the new `@`. On that start a service marks the undo
point read-only. One restart.

**From Settings, ext4.** The live root cannot be replaced while it runs, so
the go-back is staged (`/var/lib/wildos/restore-points/pending`) and the
undo point is taken first, while the system still runs. At the next start,
a dracut hook in the installed system's own initramfs applies it after the
disk is unlocked and before anything uses the root: rsync from the point
back onto `/sysroot` and `/boot`, then the carry-over. The disk password
is asked once. If the point's kernel differs from the one running, the
hook restarts once more so the restored kernel starts (with a second
password prompt); otherwise start-up simply continues.

**From start-up recovery (both layouts).** A broken system must not be
trusted to repair itself, so start-up recovery runs in a separate system:
the reset copy (below), started from GRUB with its own kernel and
initramfs from `/boot/wildos-recovery/`, mounting the image from inside the
encrypted disk (`root=live:UUID=...`, `rd.luks.uuid=...`). Nothing of the
installed system runs. It shows 17.3 and 17.4 and applies restore points
with the same code:

- Btrfs: "Start the earlier version", with the 20 second countdown. It does
  the subvolume swap above and restarts.
- ext4: "Restore the earlier version", "Takes a few minutes", no countdown.
  It copies the point back with rsync and restarts.

Starting this system unlocks the disk with the disk password, on the usual
start-up screen; there is no way to read the restore points without it.
The restart afterwards asks for the password again: the key never leaves
the recovery system's memory, and Wild OS does not keep it anywhere else.

**What stays when going back** (the 17.2 promise, checked path by path):

| Promise | Where it lives | How it stays |
| --- | --- | --- |
| Your files | `/home` | separate subvolume, or excluded on ext4 |
| Mail, Calendar, Notes | in `/home` | as above |
| Your settings | in `/home`; time zone, language, keyboard and computer name in `/etc` | the `/etc` files are carried over |
| Passwords | account passwords in `/etc/shadow`; fingerprints in `/var/lib/fprint`; the disk password in the LUKS header | accounts carried over (below); `/var/lib/fprint` carried; the LUKS header is never part of a restore point |
| Wi-Fi | `/etc/NetworkManager/system-connections`, `/var/lib/NetworkManager` | carried over |
| Windows mode and its apps | see point 2 | never snapshotted |

Bluetooth pairings (`/var/lib/bluetooth`) and account pictures
(`/var/lib/AccountsService`) are carried too. Accounts are merged rather
than copied: the restore point's `/etc/passwd`, `group`, `shadow` and
`gshadow` keep their system accounts, and every person's account (UID 1000
and up) with its password and group memberships comes from the current
system, so an account made or a password changed after the restore point
survives.

What goes back: everything else in the system, which is the point: system
files, drivers, apps from the Store (Flatpak's system installation is in
`/var/lib/flatpak`) and their versions.

**How an ext4 copy stays quick.** Each point is first hard-linked to the
previous one by size and time (to the nanosecond), then every file whose
inode changed since the previous point started (its ctime) is compared by
content. Time alone is not enough: Wild OS packages give every file the
changelog's date, so two builds can differ in a file of the same size and
time. Going back always compares contents. Measured on the ext4 test VM
(Alpha 2, qcow2 on the dev PC): the first point 76 seconds and 5.8 GB,
the point before installing a package 17 seconds and 49 MB.

## Taking restore points

- **Before every apt run that changes packages**: `DPkg::Pre-Invoke` in
  `/etc/apt/apt.conf.d/80wildos-restore-points`. libapt runs it for apt,
  PackageKit (Settings > Updates, the Store) and unattended-upgrades alike.
  `DPkg::Pre-Install-Pkgs` then reads what is about to change and names the
  point: a Wild OS build ("Before the build 331 update"), a driver, or the
  apps. Automatic security updates run apt several times in a row; one
  point covers a run of them (no new automatic point within 30 minutes of
  the last).
- **Before installing a driver**: Settings' graphics driver helper sets
  `WILDOS_RESTORE_LABEL="Before installing the graphics driver"`, which the
  hook uses. It is the only place Wild OS installs drivers.
- **On demand**: "Create a restore point" in Settings.
- **The first one**: right after this package is installed, in the
  background, so that the first update does not wait for a full copy on
  ext4.

The hook never stops apt: any failure is logged and the update carries on.
It does nothing in a chroot, in the image build, in the installer or on the
live system.

**Hold back the update for 2 weeks.** Ticked by default in 17.2. It writes
`/etc/apt/preferences.d/wildos-hold-back` with a `Pin-Priority: -1` for the
exact versions that update brought from deb.wildos.io, taken from the dpkg
log, with an expiry date that the hook and a daily timer remove. Debian's
security fixes have other versions and origins, so they still install; a
later Wild OS build is not held either, since it may be the fix.

## Point 4: the reset copy

The reset copy is the release's own compressed system image, its kernel and
live initramfs, and the installer's small GRUB package pool, kept in
`/var/lib/wildos/reset/` (Btrfs: the `@reset` subvolume). About 2.1 GB. It
does three jobs: Reset Wild OS, start-up recovery's system, and the source
for "Reset" when offline.

**Signed.** The ISO build writes `LiveOS/reset.sha256` (SHA-256 of the
image, kernel, initramfs and pool packages) and, when the Wild OS updates
signing key is available, `LiveOS/reset.sha256.sig`, a detached signature
by the same key as the update channel
(`packages/wildos-base/files/apt/wildos-updates-keyring.asc`). Before every
use (every reset, and before the recovery files are written to `/boot`)
the tool checks the signature with `gpgv` against that keyring and every
file against the list. A copy that fails is not used: Reset says the copy
is damaged and offers to fetch a new one.

**Where it comes from.**
- New installs: the installer copies it from the USB stick into the new
  system, right after `unpackfs`.
- Every point release replaces it: a daily timer reads the published
  release's `SHA256SUMS` and its signature on dl.wildos.io, and when the
  release is newer than the copy, downloads the ISO in the background at
  idle priority (never on a metered connection), checks the ISO against the
  signed sums, takes the files out, checks them against the signed list
  inside, and only then swaps the old copy for the new one. A reset never
  lands on an old system, and only one copy is ever kept.
- ext4 installs made before this change get theirs the same way, from the
  first release built with this change. Until then Settings shows Reset as
  "Getting ready" with the reason.

`/boot/wildos-recovery/` holds the recovery system's kernel and initramfs
(GRUB has to read them), written from the verified copy.

## Start-up recovery (17.3, 17.4)

**When it appears.**
- Two failed starts in a row. GRUB counts starts in `grubenv` (on the
  unencrypted ext4 `/boot`, which GRUB can write): GRUB has no
  arithmetic, so the count is a string, `""`, `1`, `2`. A start only counts
  as good once a person's session reaches the desktop:
  `wildos-boot-ok.service` watches logind for a graphical session that is
  not the greeter and has been up for 20 seconds, then clears the count.
  On the third start GRUB boots start-up recovery instead.
- Shift held while the PC starts: the menu script checks `keystatus
  --shift` and starts recovery. Esc still shows the GRUB menu (Windows,
  firmware settings, safe graphics, restore points).
- "Restart now" under Start-up options in Settings (`grub-reboot`).

**What it is.** The reset copy started live with `wildos.mode=recovery`:
`wildos-live`'s session runs the recovery app alone in KWin, as install
mode runs the installer. Black, as the boot screen: the keyboard layout chip
bottom left, accessibility and power bottom right, full keyboard navigation
(arrows, Enter, Esc for Back). The app only offers what fits the disk:
"Start the earlier version" with a 20 second countdown on Btrfs, "Restore
the earlier version" / "Takes a few minutes" on ext4.

**More options:** choose an earlier version (every restore point), safe mode
(the installed system with `nomodeset`, NVIDIA's modules blocked and
startup apps skipped by a user generator that reads `wildos.safe=1`), Reset
Wild OS, Repair tools (check the disk read-only, repair start-up: reinstall
GRUB, rebuild the initramfs and the menu; a terminal) and Report a problem
(`wildos-report --save <USB stick> --part startup`, from the report app on
branch `report-problem`; the option says so when the app is missing).

Anything that changes the system asks for the disk password again first
(checked with `cryptsetup open --test-passphrase`), so nobody who finds the
PC at that screen can reset it.

## Reset Wild OS (17.5)

Opened from Settings (Reset) or from More options at start-up. Three steps
in one dialog: choose, what happens, confirm.

**Keep my files.** Runs in the recovery system after one restart. The
system is copied back from the verified reset copy with rsync over `@` (or
the ext4 root), excluding everything the "kept" list promises: `/home`,
Windows mode (point 2), Wi-Fi, accounts (merged as above, so passwords
stay), fstab, crypttab and the encryption marker, the computer name and
regional settings, restore points and the reset copy. Then, as the
installer's last step does: the live-only packages are removed with apt
(and what only they needed), the GRUB package for this firmware is
installed from the copy's pool, the initramfs and menu are rebuilt.

The installed system's own tools run in a chroot of it, set up as at start:
on Btrfs `@` is mounted on its own (not entered as a folder of the top
level), with `@log`, `@cache`, `@tmp`, `@reset` and `@snapshots` at their
places. dracut only puts in the initramfs what it sees the running system
use; inside a folder of the top level it saw no root device, and the
initramfs it made had no encryption and no Btrfs driver, so the PC stopped
at a black screen before the disk password (the VM, 12 November). After
dracut, every kernel's initramfs is checked for the encryption module (when
crypttab has an entry) and the root file system's driver; one that lacks
them is made again without host-only mode, and if that fails too the reset
says so and offers the restore point taken before it. Repair start-up uses
the same check. Without `@reset` in the chroot, the recovery files were
deleted from `/boot` and the menu lost start-up recovery. Before any of it, a list of the apps installed by the
person (apt's manual installs that are not in the image, and Flatpak apps)
is written to each person's Desktop as "Apps before the reset.txt". Updates
since the copy's release install as usual afterwards. A restore point is
taken before the reset, so even a reset can be undone.

**Remove everything.** Confirmed twice. Restarts into the recovery system
with the image copied into memory (`rd.live.ram=1`), which then wipes the
disk and runs the installer's own erase-and-encrypt path from that image,
so a new disk password and a new recovery key are set exactly as on a new
install, and first-run setup follows. It is the only path that touches
personal files.

It needs 3.5 GiB of `MemTotal` (a PC with 4 GB fitted): the image alone is
2.2 GB. Settings and start-up recovery show the choice greyed out with the
reason below that, and `recovery-env reset-everything` refuses it.

dracut leaves the disk the image came from mounted at
`/run/initramfs/live` even with `rd.live.ram=1`, so "in memory" is
`/run/initramfs/squashed.img` existing, not that mount being gone.
`prepare-wipe` copies the rest of the reset copy (start-up recovery's
kernel and initramfs, the signed list, `.disk/info`, the pool) into memory,
unmounts the disk everywhere, including the private mount namespaces of
services that started with it (jitterentropy-rngd held it open in the VM),
closes it, and lays the copy out at `/run/initramfs/live` as the installer
expects from a USB stick. The installer's resetcopy step then gives the
new install a complete, verified reset copy and start-up recovery.

## Root helpers and polkit

One helper, `/usr/libexec/wildos/settings-recovery`, behind
`org.wildos.settings.recovery` (admin password, `auth_admin_keep`, like the
other Settings helpers). It accepts only: `create <label>`, `go-back <id>
[--hold]`, `delete <id>`, `limits <count> <gb> <on|off>`, `restart-recovery`
and `reset keep|everything`. Ids are checked against the index; labels are
plain text with a length limit. Listing needs no password.

## Privacy

Nothing leaves the PC. Restore points, the index and the reset copy all
live inside the encrypted disk; the only network use is downloading the
public release image. No telemetry of any kind.

## Not done here, and why

- **The second password after start-up recovery** is the cost of not
  keeping the disk key anywhere. A TPM-sealed key would remove it, which is
  a separate decision.
- **Converting existing ext4 installs to Btrfs** in place is not attempted:
  too risky on an encrypted disk with no backup. They get file restore
  points.

## Tested (12 November, VMs)

- **ext4, Alpha 2 install upgraded to these packages:** automatic point
  before apt; going back from the Settings helper (package gone, /home
  untouched, one restart, one disk password); undo (package back, a home
  file written after the go-back kept); pruning at 5. Clean points: first
  76 s and 5.8 GB, then about 17 s and 50 MB before an apt run.
- **Btrfs, fresh install from a test ISO:** the ten subvolumes, copy-on-write
  off for Windows mode, libvirt and the swap file, `rootflags=subvol=@`, the
  recovery key; automatic snapshot before apt (0.1 s); PackageKit (the path
  Settings > Updates and the Store use) takes a point first; online go-back
  (0.4 s, one restart) and undo; the start-up recovery code path run offline
  against the disk; two failed starts start start-up recovery from the reset
  copy, whose signature and sums are checked (3 s for 2.1 GB); choosing an
  earlier version there with the disk password; starting a restore point
  from the start-up menu.
- **Reset, Keep my files (Btrfs VM, 12 November):** the first run found
  three bugs: update-grub failed in the chroot, the image's live account
  took GID 1000 from Windows mode's kvmsf group, and the PC stopped at a
  black screen after the reset. The black screen was the initramfs: the
  reset ran with the chroot entered as a folder of the Btrfs top level, so
  dracut found no root device and left out encryption, device mapper and
  Btrfs (no disk password prompt, then `rd.emergency=halt`). The "Failed to
  open drm node" line was from the boot before the reset; KWin logs it on
  every start in this VM (virtio-gpu without 3D has no render node) and it
  was never the cause. The reset system never wrote a journal. Run again
  with this code from start-up recovery: restore point "Before resetting
  Wild OS", app list on the Desktop, the person's package and Calamares'
  libraries gone, initramfs with crypt, dm and btrfs, start-up recovery and
  the restore points still in the menu; disk password, sign-in screen,
  sign-in with the old password, desktop, home file kept, and the start
  counted as good.
- **Remove everything (same VM):** restart into start-up recovery from
  memory, the wipe screen, the disk password, the installer, Replace
  everything with a new disk password, a new recovery key; the old password
  no longer opens the disk, the new LUKS header has a new UUID, first-run
  setup starts, and the new install's reset copy is complete and verified
  with `/boot/wildos-recovery` in place.
- **Both from the screens, ext4 (the Alpha 2 VM with these packages and a
  reset copy repacked with them, signed with the test key):** Settings >
  Recovery > Reset > Keep my files, the account password, the restart into
  start-up recovery on its last step, the disk password, then the sign-in
  screen with "Encrypted and locked", sign-in with the old password, a home
  file kept, three apps listed on the Desktop, the restore point before the
  reset listed in Settings, the start counted as good. Then Settings >
  Remove everything with its tick box, the restart into start-up recovery
  from memory, the wipe, the installer and a new disk password: the old one
  no longer opens the disk, the new install is Btrfs with a verified reset
  copy, and its `/etc/wildos/encrypted` marker is there (the installer
  missed it on every Btrfs install before 0.1.4).
- **Not proven:** a PC with less than 4 GB (Remove everything greyed out),
  the full-initramfs fallback (the host-only one passed the check every
  time), Report a problem to a USB stick (needs wildos-report on the
  installed system), Check the disk and Repair start-up, Shift at start-up (QEMU's keys never read as held; the menu entry
  and Settings' Restart now reach the same screen), a real release signed
  with the release key (tests used a throwaway key).
