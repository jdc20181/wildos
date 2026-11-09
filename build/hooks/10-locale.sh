# English (UK) until someone picks another language on the start-up menu,
# the same default the installer, first start and Settings use. The
# installer takes the live session's LANG, so an en_US here installed every
# PC with a US keyboard and New York time. Every language the menu offers
# (wildos-live's list) needs a generated UTF-8 locale, or Plasma and Qt apps
# fall back to ASCII.
for code in $(grep -v '^#' /usr/share/wildos-live/languages | cut -f 1); do
    sed -i "s/^# *\($code\.UTF-8 UTF-8\)/\1/" /etc/locale.gen
done
locale-gen
update-locale LANG=en_GB.UTF-8

echo wildos > /etc/hostname
