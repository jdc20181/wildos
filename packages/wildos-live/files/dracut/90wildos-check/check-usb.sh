#!/bin/sh
# Runs "Check this USB stick": reads the whole stick, compares it with the
# checksum implanted when the ISO was made, and shows the result in words.
# Exits 0 to carry on starting Wild OS; a failed stick turns the computer off
# instead of starting from damaged files.

dev=$1
result=/run/wildos-check-result

printf '\033[2J\033[H\n\n'
printf '  Checking this USB stick\n\n'
printf '  Wild OS is reading the whole stick to find download or copying errors.\n'
printf '  This takes a few minutes.\n\n'

# checkisomd5 --gauge prints one percentage per line, then its own summary,
# which is left out for the plainer words below.
{ checkisomd5 --gauge "$dev" 2>/dev/null; echo $? > "$result"; } | while read -r line; do
    case $line in
        ''|*[!0-9]*) ;;
        *) printf '\r  %s%% checked ' "$line" ;;
    esac
done
status=$(cat "$result" 2>/dev/null)

case $status in
    0)
        printf '\n\n  This USB stick is fine.\n\n'
        printf '  Press Enter to start Wild OS.'
        read -r _
        ;;
    1)
        printf '\n\n  This USB stick has errors.\n\n'
        printf '  Wild OS might not start or install correctly from it. Download\n'
        printf '  Wild OS again from wildos.io and copy it to the stick again.\n\n'
        printf '  Press Enter to turn off this computer.'
        read -r _
        systemctl --no-block poweroff
        sleep infinity
        ;;
    *)
        # No checksum on the stick (a hand-made copy), or the check was
        # stopped: nothing to report, so start as usual.
        ;;
esac
printf '\033[2J\033[H'
exit 0
