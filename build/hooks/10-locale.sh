# English (US) is the fallback until the installer asks; the live session needs
# a generated UTF-8 locale or Plasma and Qt apps fall back to ASCII.
sed -i 's/^# *\(en_US.UTF-8 UTF-8\)/\1/; s/^# *\(en_GB.UTF-8 UTF-8\)/\1/' /etc/locale.gen
locale-gen
update-locale LANG=en_US.UTF-8

echo wildos > /etc/hostname
