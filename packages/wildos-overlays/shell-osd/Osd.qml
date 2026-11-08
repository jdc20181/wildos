// Plasma 6.3 loads the on-screen display only from the shell package, not
// from the look-and-feel package, so this stands in for the shell's own
// Osd.qml (diverted by wildos-overlays' preinst) and loads the Wild OS one.
import QtQuick
import "file:///usr/share/plasma/look-and-feel/org.wildos.desktop/contents/osd" as WildOS

WildOS.Osd {}
