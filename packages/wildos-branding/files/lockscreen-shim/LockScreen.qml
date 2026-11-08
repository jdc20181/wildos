// Plasma 6.3 loads the lock screen only from the shell package, not from the
// look-and-feel package, so this stands in for the shell's own LockScreen.qml
// (diverted by wildos-branding's preinst) and loads the Wild OS one.
import QtQuick
import "file:///usr/share/plasma/look-and-feel/org.wildos.desktop/contents/lockscreen" as WildOS

WildOS.LockScreen {}
