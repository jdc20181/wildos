import QtQuick
import WildOS.UI as UI

// An icon on the desktop: This computer, the Home folder and the Recycle
// Bin as drawn in DesktopIcons.dc, everything else from the Wild OS icon
// theme, with pictures and videos showing what is in them.
Item {
    id: icon

    property string kind: "item"
    property string iconName
    property url url
    property bool preview: false
    property var stamp: 0
    property real size: 48
    property bool binFull: false
    property bool faded: false

    readonly property string glyph: kind === "computer" ? "computer"
        : kind === "home" ? "user-folder"
        : kind === "bin" ? (binFull ? "bin-full" : "bin")
        : ""

    implicitWidth: size
    implicitHeight: size
    opacity: faded ? 0.5 : 1

    Image {
        anchors.fill: parent
        visible: icon.glyph.length > 0
        source: icon.glyph.length > 0 ? Qt.resolvedUrl("glyphs/" + icon.glyph + ".svg") : ""
        sourceSize: Qt.size(icon.size * 2, icon.size * 2)
        smooth: true
        mipmap: true
    }

    UI.ThemeIcon {
        anchors.fill: parent
        visible: icon.glyph.length === 0 && !thumb.ready
        name: icon.glyph.length === 0 ? icon.iconName : ""
        fallback: "text-x-generic"
    }

    Thumbnail {
        id: thumb
        anchors.fill: parent
        visible: ready
        url: icon.preview && icon.glyph.length === 0 ? icon.url : ""
        stamp: icon.stamp
    }
}
