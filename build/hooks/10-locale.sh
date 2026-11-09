# English (UK) until someone picks another language, the same default the
# installer, first start and Settings use (the start-up menu passes no
# language yet). The installer takes the live session's LANG, so an en_US
# here installed every PC with a US keyboard and New York time. The live
# session needs a generated UTF-8 locale or Plasma and Qt apps fall back to
# ASCII.
sed -i 's/^# *\(en_US.UTF-8 UTF-8\)/\1/; s/^# *\(en_GB.UTF-8 UTF-8\)/\1/' /etc/locale.gen
locale-gen
update-locale LANG=en_GB.UTF-8

echo wildos > /etc/hostname
