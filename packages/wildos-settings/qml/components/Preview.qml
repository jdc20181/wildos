import QtQuick
import WildOS.UI as UI

// The desktop preview from Personalisation, or with lockScreen set, the
// lock screen: the picture with the clock and date over it.
DesktopPreview {
    id: preview

    property bool lockScreen: false

    sketch: !lockScreen

    Column {
        visible: preview.lockScreen
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height * 0.22
        spacing: 2

        UI.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Format.time(new Date())
            size: 40
            font.weight: UI.Style.weightLight
            color: UI.Style.inkOnWallpaper
        }

        UI.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Format.longDate(new Date())
            size: 11
            color: UI.Style.inkOnWallpaper
        }
    }
}
