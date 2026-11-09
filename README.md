<div align="center">

<a href="https://wildos.io">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/banner-dark.webp">
    <source media="(prefers-color-scheme: light)" srcset="docs/assets/banner-light.webp">
    <img alt="Wild OS. Linux privacy, Windows ease." src="docs/assets/banner-dark.webp" width="100%">
  </picture>
</a>

### A hardened Linux desktop, rebuilt from scratch, with your Windows apps built in.

**[Website](https://wildos.io)** · **[Take the tour](https://wildos.io/tour/)** · **[How it works](https://wildos.io/how-it-works/)** · **[FAQ](https://wildos.io/faq/)** · **[Download](https://wildos.io/faq/)**

[![Release](https://img.shields.io/github/v/release/wildorigins/wildos?include_prereleases&label=release&style=flat-square)](https://github.com/wildorigins/wildos/releases)
[![Licence](https://img.shields.io/badge/licence-GPL--3.0--or--later-3a56c4?style=flat-square)](LICENSE)
[![Debian 13](https://img.shields.io/badge/base-Debian%2013-a80030?style=flat-square)](https://www.debian.org)
[![KDE Plasma](https://img.shields.io/badge/engine-KDE%20Plasma%206-5a72c8?style=flat-square)](https://kde.org/plasma-desktop/)

</div>

> [!IMPORTANT]
> Wild OS is in early alpha. Alpha 1 is out now: [download it](https://wildos.io/faq/), and back up anything that matters before you install.

## What is Wild OS?

Wild OS is a free, open source operating system for people who want the privacy of Linux without giving up the programs and habits they already have.

It is built in three layers, and each one has a single job.

| Layer | What it does |
| --- | --- |
| **Hardened foundation** | Debian 13 with an established security hardening layer: kernel and sysctl hardening, AppArmor, USB device control and RAM wiped on shutdown. Full-disk encryption is on from install, and updates are signed. |
| **The Wild desktop** | KDE's proven engine underneath, with every screen you touch redesigned: Start, the taskbar, Settings, Files, the Store, the installer, first start and the system dialogs. Not a theme, a rebuilt interface. |
| **Windows mode** | A genuine copy of Windows 11 Pro running out of sight. Each Windows program opens as its own normal window on your desktop. |

There's no account, no telemetry and no adverts. The full story is at **[wildos.io](https://wildos.io)**.

## Screens

These are the Wild OS designs every screen is built to.

<table>
  <tr>
    <td width="50%"><img src="docs/assets/screens/main.webp" alt="The Wild OS desktop with Files open and a Windows app alongside"><br><sub><b>The desktop.</b> A Windows app running next to Files.</sub></td>
    <td width="50%"><img src="docs/assets/screens/start.webp" alt="The Start menu with pinned apps and recent files"><br><sub><b>Start.</b> Pinned apps, recent files and search.</sub></td>
  </tr>
  <tr>
    <td><img src="docs/assets/screens/winapp.webp" alt="Installing a Windows program in Windows mode"><br><sub><b>Windows apps.</b> Double-click an installer and it just installs.</sub></td>
    <td><img src="docs/assets/screens/installerencryption.webp" alt="The installer's encryption step, on by default with a recovery key"><br><sub><b>Installer.</b> Encryption on by default, explained in one sentence.</sub></td>
  </tr>
</table>

<details>
<summary><b>More screens</b></summary>
<br>

| | |
| --- | --- |
| ![Light mode desktop](docs/assets/screens/mainlight.webp) | ![Setting up Windows mode](docs/assets/screens/winmodesetup.webp) |
| ![Snap layouts](docs/assets/screens/snap.webp) | ![Settings](docs/assets/screens/settings.webp) |
| ![Shutting down with unsaved work](docs/assets/screens/winmodeshutdown.webp) | ![A permission prompt](docs/assets/screens/dialogpermission.webp) |
| ![First start welcome](docs/assets/screens/welcome.webp) | ![Task Manager](docs/assets/screens/taskmanagerperf.webp) |
| ![The Store](docs/assets/screens/store.webp) | ![Alt+Tab](docs/assets/screens/alttab.webp) |
| ![The lock screen](docs/assets/screens/lock.webp) | ![The installer](docs/assets/screens/installer.webp) |
| ![Windows programs found at first start](docs/assets/screens/firstwinapps.webp) | ![Camera and microphone for Windows apps](docs/assets/screens/winmodecalls.webp) |

Or click through every screen yourself in the **[interactive tour](https://wildos.io/tour/)**.

</details>

## Windows mode

Windows mode doesn't imitate Windows. It runs a real, licensed copy of Windows 11 Pro in a hidden virtual machine and shows each program as its own window, with its own taskbar icon, in Alt+Tab and snapping like everything else. There is no Wine: one engine is simpler and more reliable than a mixed bag.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/assets/windows-mode-flow-dark.svg">
  <source media="(prefers-color-scheme: light)" srcset="docs/assets/windows-mode-flow-light.svg">
  <img alt="How Wild OS handles a Windows program: wild-run sends most programs to Windows mode, which shows them as normal windows on your desktop; programs with a Linux version, like Zoom, Slack and Steam, install that from the Store; games with kernel anti-cheat aren't supported, and Wild OS says so." src="docs/assets/windows-mode-flow-light.svg" width="100%">
</picture>

- **Light when you're not using it.** A minute after you close your last Windows app it pauses, using no processor time. On battery or short of memory it saves itself to disk instead.
- **Never lose work.** Shut down with an unsaved document and Windows is kept exactly as it was, then reopened next time.
- **Your files stay yours.** Windows only sees the folders you share. Saving in Word puts the file straight into your Documents folder.
- **Sealed off.** Its own private network, no view of your home network, and snapshots so a bad Windows update can be rolled back.

### What you need

> [!NOTE]
> Windows mode needs a **Windows 11 Pro** licence. Most PCs sold with Pro have the key stored in the firmware, and Wild OS finds and uses it. **Windows 11 Home isn't supported yet**, because the feature that shows programs as separate windows is only in Pro. We're working on our own way to support Home.

Everything else in Wild OS works on any PC, with or without a Windows licence.

### Compatibility

| Software | Works | Notes |
| --- | :---: | --- |
| Microsoft 365 and Office | ✅ | Word, Excel, PowerPoint, Outlook, OneNote |
| Adobe Creative Cloud | ✅ | Photoshop, Illustrator, Acrobat. Heavy 3D work is slower |
| Business and accounts software | ✅ | Accounts packages, line-of-business and industry tools |
| Everyday Windows programs | ✅ | Anything that installs from an .exe or .msi |
| Video calls | ✅ | Zoom and Slack install their Linux versions, and Teams runs as its web app |
| Windows games | ✅ | Run through Steam's Proton without Windows mode |
| Games with kernel anti-cheat | ❌ | Valorant, Fortnite and a few others refuse Linux and virtual machines |
| Windows-only hardware drivers | ❌ | Some dongles and scanners. Device passthrough is planned |

Tested something? [Open an issue](https://github.com/wildorigins/wildos/issues) so others know.

## System requirements

| | Minimum | Recommended |
| --- | --- | --- |
| Processor | 64-bit Intel or AMD | Any recent Intel Core or AMD Ryzen |
| Memory | 4 GB | 16 GB, especially for Windows mode |
| Storage | 40 GB | 128 GB SSD or more |
| Firmware | UEFI or legacy BIOS | UEFI with Secure Boot |
| Windows mode | Windows 11 Pro licence, virtualisation on | A PC sold with Windows 11 Pro |

## Try it

> [!WARNING]
> Alpha builds are for testing. Try them from the USB stick or in a virtual machine before installing over anything you care about.

1. **Download** the ISO from [wildos.io/download/iso](https://wildos.io/download/iso) and its checksums from [wildos.io/download/checksums](https://wildos.io/download/checksums). The checksums are signed with the Wild OS key in [packages/wildos-base/files/apt/wildos-updates-keyring.asc](packages/wildos-base/files/apt/wildos-updates-keyring.asc).
2. **Verify** the download.
   ```sh
   sha256sum -c SHA256SUMS
   ```
3. **Write it to a USB stick** (8 GB or larger) with [balenaEtcher](https://etcher.balena.io), [Rufus](https://rufus.ie) on Windows, or from Linux:
   ```sh
   sudo dd if=wildos-amd64.iso of=/dev/sdX bs=4M status=progress oflag=sync
   ```
4. **Start your PC from the stick.** It boots on UEFI, with Secure Boot on, and on legacy BIOS. Choose **Try Wild OS** to look around without touching your disk, or **Install Wild OS** to begin.

The installer can put Wild OS alongside Windows, so you can choose which to start each time.

## Building from source

You need Docker. On Windows that's Docker Desktop with the WSL 2 backend.

```sh
git clone https://github.com/wildorigins/wildos.git
cd wildos
./build-iso.sh        # Linux or macOS
.\build-iso.ps1       # Windows PowerShell
```

The ISO is written to `out/`. The first build downloads about 3 GB of packages; later builds reuse a cache. Released ISOs and updates are built from this source, so anyone can check what went into them.

## Repository layout

| Path | What's in it |
| --- | --- |
| `build/` | Image build: builder container, package list, chroot hooks, boot menu, patches to upstream packages |
| `packages/` | Wild OS's own Debian packages: base system, desktop and shell, Settings, Files, Store, Terminal, Task Manager, welcome, installer and Windows mode (`wildos-winmode`) |
| `branding/` | Logo, glyphs and wallpaper |
| `docs/assets/` | The images on this page |

## Roadmap

- [x] Bootable live ISO and installer, with Secure Boot and encryption by default
- [x] The Wild desktop: Start, taskbar, Settings, Files, Store, welcome, light and dark
- [x] Signed update channel with stable and beta
- [x] Windows mode, first alpha, for PCs with Windows 11 Pro
- [ ] **Now:** Alpha testing and fixes on real hardware
- [ ] **Later:** Windows 11 Home support, device passthrough, Strict and Maximum protection levels

## Contributing

Wild OS is built in the open and help is welcome, from code to testing Windows apps to fixing a typo.

- Ask questions, suggest ideas and show off your setup in [Discussions](https://github.com/wildorigins/wildos/discussions).
- Report bugs and compatibility results through [Issues](https://github.com/wildorigins/wildos/issues).
- Found a security problem? Please don't open a public issue. [Report it privately](https://github.com/wildorigins/wildos/security/advisories/new) instead.

## Credits

Wild OS stands on the work of [Kicksecure](https://www.kicksecure.com) and the [Whonix](https://www.whonix.org) project, whose security packages it installs unmodified from their repository. Wild OS is an independent project and is not affiliated with or endorsed by them; Kicksecure is a trademark of its owners. It is built on [Debian](https://www.debian.org), [KDE](https://kde.org), [Calamares](https://calamares.io), [QEMU and KVM](https://www.qemu.org), [libvirt](https://libvirt.org) and [FreeRDP](https://www.freerdp.com). Thank you.

The default wallpaper is a photograph by Quang Nguyen Vinh. Artwork credits and licences are in [branding/CREDITS.md](branding/CREDITS.md).

## Licence

Wild OS's own code is licensed under the [GNU General Public License v3.0 or later](LICENSE). Packages from Debian, Kicksecure and other upstreams keep their own licences.

<sub>A Wild Origins project. Wild OS is not affiliated with Microsoft. Windows mode needs your own Windows licence, and Windows itself isn't included in Wild OS. Windows and Microsoft 365 are trademarks of Microsoft Corporation.</sub>
