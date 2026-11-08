import QtQuick
import QtQuick.Window
import WildOS.UI as UI
import WildOS.SystemDialogs

// The dimmed, blurred cover on a screen other than the one with the
// dialog, so nothing on any screen can be clicked or read past it.
Window {
    id: window

    property bool open: false
    property real dim: 0.55

    visible: false
    color: "transparent"

    Surface {
        id: surface
        window: window
        role: "backdrop"
        scope: "wildos-backdrop"
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(UI.Style.wallpaperDim.r, UI.Style.wallpaperDim.g, UI.Style.wallpaperDim.b, surface.blurAvailable ? window.dim : Math.min(0.9, window.dim + 0.3))
    }

    onOpenChanged: open ? surface.show() : surface.hide()
}
