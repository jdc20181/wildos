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
