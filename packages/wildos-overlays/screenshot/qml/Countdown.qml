import QtQuick
import QtQuick.Window
import WildOS.UI as UI
import org.wildos.overlays
import WildOS.Screenshot

// The seconds left before a delayed capture, in the centre of the main
// screen in the same 160 by 120 square as the Caps Lock indicator. It lets
// clicks through, so the menu or hover being set up stays undisturbed.
Window {
    id: countdown

    width: 160
    height: 120
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput
    visible: ScreenshotApp.countdown > 0
    title: "Screenshot countdown"

    OverlayPlacement {
        window: countdown
        layer: OverlayPlacement.Overlay
        scope: "wildos-screenshot-countdown"
    }

    UI.WindowFrame {
        window: countdown
        radius: 20
        shadowColor: UI.Style.shadowWindow
    }

    Rectangle {
        anchors.fill: parent
        radius: 20
        color: UI.Style.glass(16, 20, 38, 0.84, 0.88)
        border.width: 1
        border.color: UI.Style.borderGlass

        Column {
            anchors.centerIn: parent
            spacing: 6

            UI.Label {
                anchors.horizontalCenter: parent.horizontalCenter
                size: 32
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
                text: ScreenshotApp.countdown
            }

            UI.Label {
                anchors.horizontalCenter: parent.horizontalCenter
                size: 13
                text: "Screenshot in"
            }
        }
    }
}
