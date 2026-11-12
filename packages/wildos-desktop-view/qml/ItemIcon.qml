import QtQuick
import WildOS.UI as UI

// An icon on the desktop, from the Wild OS icon theme: This computer, the
// Home folder and the Recycle Bin by their standard names (the theme takes
// them from Fluent, This computer with the dawn wallpaper), everything else
// by its file's icon, with pictures and videos showing what is in them.
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

    readonly property string systemIcon: kind === "computer" ? "computer"
        : kind === "home" ? "user-home"
        : kind === "bin" ? (binFull ? "user-trash-full" : "user-trash")
        : ""

    implicitWidth: size
    implicitHeight: size
    opacity: faded ? 0.5 : 1

    UI.ThemeIcon {
        anchors.fill: parent
        visible: icon.systemIcon.length > 0 || !thumb.ready
        name: icon.systemIcon.length > 0 ? icon.systemIcon : icon.iconName
        fallback: "text-x-generic"
    }

    Thumbnail {
        id: thumb
        anchors.fill: parent
        visible: ready
        url: icon.preview && icon.systemIcon.length === 0 ? icon.url : ""
        stamp: icon.stamp
    }
}
