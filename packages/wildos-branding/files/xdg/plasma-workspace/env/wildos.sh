# Sourced by startplasma before anything in the session starts; Plasma then
# hands the resulting environment to systemd and D-Bus, so every app sees it.

# /usr/share/wildos holds the renamed launchers and the reworded interface
# text. systemd's environment.d puts it first, but Kicksecure's profile.d
# script rewrites XDG_DATA_DIRS during login and startplasma imports that
# version over it, so it is put back here.
case ":${XDG_DATA_DIRS:-}:" in
    *:/usr/share/wildos:*) ;;
    *) export XDG_DATA_DIRS="/usr/share/wildos:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" ;;
esac

# Wild OS is written in UK English. KDE treats en_US as its source language
# and never reads a translation for it, so English locales read the en_GB
# text, which is where Wild OS's wording lives. Other languages are left alone.
if [ -z "${LANGUAGE:-}" ]; then
    case "${LANG:-C}" in
        en_*|C|C.*|POSIX) export LANGUAGE=en_GB:en ;;
    esac
fi

# Times use the 24-hour clock and dates are day-first everywhere in Wild OS.
# Accounts on the US English or plain C locale that have not picked their own
# date format get the UK one, so file dates read 06/11/2026 13:10 rather than
# 11/6/26 1:10 PM.
if [ -z "${LC_TIME:-}" ] && [ -z "${LC_ALL:-}" ]; then
    case "${LANG:-C}" in
        en_US*|C|C.*|POSIX)
            if locale -a 2>/dev/null | grep -qix 'en_GB\.utf-\{0,1\}8'; then
                export LC_TIME=en_GB.UTF-8
            fi
            ;;
    esac
fi

# The Files sidebar for a new account: the personal folders in File
# Explorer's order, without KDE's Network, Recent and Search sections. Files
# would otherwise write KDE's own list on first start.
places="${XDG_DATA_HOME:-$HOME/.local/share}/user-places.xbel"
if [ ! -e "$places" ] && [ -r /usr/share/wildos/places/user-places.xbel ]; then
    command -v xdg-user-dirs-update >/dev/null && xdg-user-dirs-update >/dev/null 2>&1
    dir() { d=$(xdg-user-dir "$1" 2>/dev/null); echo "${d:-$HOME/$2}"; }
    mkdir -p "$(dirname "$places")"
    sed -e "s|@HOME@|$HOME|" \
        -e "s|@DESKTOP@|$(dir DESKTOP Desktop)|" \
        -e "s|@DOCUMENTS@|$(dir DOCUMENTS Documents)|" \
        -e "s|@DOWNLOAD@|$(dir DOWNLOAD Downloads)|" \
        -e "s|@PICTURES@|$(dir PICTURES Pictures)|" \
        -e "s|@MUSIC@|$(dir MUSIC Music)|" \
        -e "s|@VIDEOS@|$(dir VIDEOS Videos)|" \
        /usr/share/wildos/places/user-places.xbel > "$places"

    # Documents opens in the Details view, as it does in File Explorer;
    # every other folder keeps the icon view. The timestamp lies in the
    # future because Files ignores folder settings older than its own first
    # start, which comes after this.
    docs=$(dir DOCUMENTS Documents)
    if [ -d "$docs" ] && [ ! -e "$docs/.directory" ]; then
        printf '[Dolphin]\nTimestamp=2100,1,1,0,0,0\nVersion=4\nViewMode=1\n' > "$docs/.directory"
    fi
    unset -f dir
    unset docs
fi
unset places
