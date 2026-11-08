import QtQuick
import QtQuick.Window
import WildOS.UI as UI
import org.wildos.overlays
import WildOS.Screenshot

// While recording: a small bar at the top of the main screen with the time
// so far and Stop, in the toolbar's glass. Win+Shift+R or Win+Shift+S
// again also stops it.
Window {
    id: bar

    readonly property var recorder: ScreenshotApp.recorder

    width: row.implicitWidth + 24
    height: 52
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    visible: recorder.recording
    title: "Recording"

    OverlayPlacement {
        window: bar
        edges: OverlayPlacement.Top
        marginTop: 24
        layer: OverlayPlacement.Overlay
        scope: "wildos-screenshot-recording"
    }

    UI.WindowFrame {
        window: bar
        radius: 16
        shadowColor: UI.Style.shadowWindow
    }

    function clock(seconds: int): string {
        const m = Math.floor(seconds / 60);
        const s = seconds % 60;
        return m + ":" + (s < 10 ? "0" : "") + s;
    }

    Rectangle {
        anchors.fill: parent
        radius: 16
        color: UI.Style.glass(16, 20, 38, 0.84, 0.88)
        border.width: 1
        border.color: UI.Style.borderGlass

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 12

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 10
                height: 10
                radius: 5
                color: UI.Style.danger
                visible: !bar.recorder.finishing
                SequentialAnimation on opacity {
                    running: bar.visible && !UI.Style.reducedMotion && !bar.recorder.finishing
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.35; duration: 700 }
                    NumberAnimation { to: 1; duration: 700 }
                }
            }

            UI.Spinner {
                anchors.verticalCenter: parent.verticalCenter
                visible: bar.recorder.finishing
                size: 14
            }

            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                size: 13
                font.features: { "tnum": 1 }
                text: bar.recorder.finishing ? "Saving" : bar.clock(bar.recorder.seconds)
            }

            UI.Button {
                anchors.verticalCenter: parent.verticalCenter
                size: "small"
                kind: "primary"
                text: "Stop"
                enabled: !bar.recorder.finishing
                onClicked: bar.recorder.stop()
            }
        }
    }
}
