FROM debian:trixie

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        apt-utils \
        build-essential \
        cmake \
        extra-cmake-modules \
        qt6-base-dev \
        qt6-declarative-dev \
        qt6-svg-dev \
        pkgconf \
        libkf6config-dev \
        libkf6coreaddons-dev \
        libkf6i18n-dev \
        libkf6jobwidgets-dev \
        libkf6kio-dev \
        libkf6kcmutils-dev \
        libkf6service-dev \
        libkf6solid-dev \
        libkf6windowsystem-dev \
        libkf6colorscheme-dev \
        libkscreen-dev \
        libkf6sonnet-dev \
        libkf6networkmanagerqt-dev \
        libkf6globalaccel-dev \
        libsdl2-dev \
        breeze-cursor-theme \
        libkf6purpose-dev \
        libkf6guiaddons-dev \
        libkf6notifications-dev \
        libkpipewire-dev \
        unicode-data \
        unicode-cldr-core \
        libpackagekitqt6-dev \
        libappstreamqt-dev \
        libflatpak-dev \
        libqtermwidget-dev \
        libutf8proc-dev \
        qt6-wayland-dev \
        qt6-wayland-dev-tools \
        plasma-wayland-protocols \
        libwayland-dev \
        libpolkit-qt6-1-dev \
        liblayershellqtinterface-dev \
        libkf6dbusaddons-dev \
        libhivex-dev \
        libkf6archive-dev \
        libkf6filemetadata-dev \
        kwin-dev \
        libcups2-dev \
        libglib2.0-dev \
        libcamel1.2-dev \
        libedataserver1.2-dev \
        liblitehtml-dev \
        qtkeychain-qt6-dev \
        libksysguard-dev \
        python3 \
        python3-pil \
        ca-certificates \
        debhelper \
        dosfstools \
        dpkg-dev \
        fonts-inter \
        grub-efi-amd64-bin \
        grub-pc-bin \
        imagemagick \
        isomd5sum \
        librsvg2-bin \
        mmdebstrap \
        mtools \
        squashfs-tools \
        xorriso \
        zstd \
    && rm -rf /var/lib/apt/lists/*

# wildos-winmode: mono's C# compiler builds wild-guest, the helper that runs
# inside Windows mode, monodis checks it only uses API Windows has, and
# python3-gi lets its tests import the service.
# A layer of its own, so adding it does not rebuild the one above.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        mono-mcs \
        libmono-system-serviceprocess4.0-cil \
        mono-utils \
        python3-gi \
        gir1.2-glib-2.0 \
    && rm -rf /var/lib/apt/lists/*

# wildos-rdp, Windows mode's window client, builds against FreeRDP 3.32
# from trixie-backports, the version the image pins (wildos-base's
# wildos-freerdp.pref); trixie's own 3.15 drew RemoteApp windows wrongly.
# wayland-protocols has the window icon protocol it speaks to KWin.
RUN echo "deb http://deb.debian.org/debian trixie-backports main" > /etc/apt/sources.list.d/backports.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends -t trixie-backports \
        freerdp3-dev \
        libwinpr3-dev \
    && apt-get install -y --no-install-recommends \
        wayland-protocols \
    && rm -rf /var/lib/apt/lists/*

# Calendar (wildos-mail) builds against evolution-data-server's calendar
# library. New build dependencies go in layers of their own like this one:
# adding one to the big list above makes Docker reinstall all of it.
RUN apt-get update \
    && apt-get install -y --no-install-recommends libecal2.0-dev \
    && rm -rf /var/lib/apt/lists/*

# Quick Look in Files (wildos-files) previews PDFs through QtQuick.Pdf; its
# QML is compiled against the module's types.
RUN apt-get update \
    && apt-get install -y --no-install-recommends qml6-module-qtquick-pdf qml6-module-qtmultimedia \
    && rm -rf /var/lib/apt/lists/*

# wildos-setup's wallet key service checks the account's password with PAM
# before it keeps a key for automatic sign-in.
RUN apt-get update \
    && apt-get install -y --no-install-recommends libpam0g-dev \
    && rm -rf /var/lib/apt/lists/*

# The repacked security-misc-shared ships emerg-shutdown and fm-shim-backend
# compiled here (build.sh); fm-shim-backend links D-Bus and libsystemd and
# finds them with pkg-config.
RUN apt-get update \
    && apt-get install -y --no-install-recommends libdbus-1-dev libsystemd-dev pkgconf \
    && rm -rf /var/lib/apt/lists/*

# The password strength hint in Settings and first boot setup scores the
# password with libpwquality.
RUN apt-get update \
    && apt-get install -y --no-install-recommends libpwquality-dev \
    && rm -rf /var/lib/apt/lists/*

# Compiles go through ccache (a volume shared by every build, see
# build-iso.sh), so a package whose code barely changed rebuilds in seconds.
RUN apt-get update \
    && apt-get install -y --no-install-recommends ccache \
    && rm -rf /var/lib/apt/lists/*
