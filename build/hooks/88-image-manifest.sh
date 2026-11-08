# Records every package the image ships, with its exact version. The
# permission prompt (wildos-system-dialogs) treats a package as part of Wild
# OS when its installed version is on this list, since it then came from
# this signed build, or when apt says that version comes from a trusted
# archive. A package's own Maintainer field proves nothing, so it is not used.
install -d /usr/share/wildos
dpkg-query -W -f='${Package}\t${Version}\n' | sort > /usr/share/wildos/image-packages
wc -l < /usr/share/wildos/image-packages
