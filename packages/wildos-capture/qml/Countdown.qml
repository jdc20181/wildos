import QtQuick
import QtQuick.Window
import WildOS.UI as UI
import org.wildos.overlays

// The seconds left before a delayed capture, in the centre of the main
// screen in the same 160 by 120 square as the Caps Lock indicator. It lets
// clicks through, so the menu or hover being set up stays undisturbed.
Window {
    id: countdown

    width: 160
    height: 120
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput
    visible: CaptureApp.countdown > 0
    title: qsTr("Capture countdown")

    OverlayPlacement {
        window: countdown
        layer: OverlayPlacement.Overlay
        scope: "wildos-capture-countdown"
    }

    UI.WindowFrame {
        window: countdown
        radius: 20
        shadowColor: UI.Style.shadowWindow
    }

    Rectangle {
        anchors.fill: parent
        radius: 20
        color: UI.Style.glassPanel
        border.width: 1
        border.color: UI.Style.borderContextMenu

        Column {
            anchors.centerIn: parent
            spacing: 6

            UI.Label {
                anchors.horizontalCenter: parent.horizontalCenter
                size: 32
                font.weight: UI.Style.weightSemiBold
                font.features: { "tnum": 1 }
                text: CaptureApp.countdown
            }

            UI.Label {
                anchors.horizontalCenter: parent.horizontalCenter
                size: 13
                color: UI.Style.inkMuted
                text: CaptureApp.recordMode ? qsTr("Recording in") : qsTr("Capturing in")
            }
        }
    }
}
