# Prints the colour groups of a KDE colour scheme file, ready to append to
# kdeglobals. Apps read their colours from kdeglobals, not from the scheme
# the ColorScheme key names, so a new account (and the live session, which
# never runs Welcome) would otherwise get KDE's light Breeze colours.
# [General] and [KDE] are left out: kdeglobals has its own.
/^\[/ { keep = ($0 ~ /^\[(Colors|ColorEffects|WM)[]:]/) }
keep
