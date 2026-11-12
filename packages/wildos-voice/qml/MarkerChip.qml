import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// A marker of a finished recording, under the buttons: its flag and the
// time, which a click jumps to.
T.Button {
    id: control

    property double time

    implicitWidth: row.implicitWidth + 22
    implicitHeight: 28
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: text

    background: Rectangle {
        radius: height / 2
        color: control.down ? UI.Style.surfacePressed : control.hovered ? UI.Style.surfaceHover : UI.Style.surfaceControl
        border.width: 1
        border.color: UI.Style.borderSubtle
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: parent.radius
        }
    }

    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 6

            VoiceIcon {
                anchors.verticalCenter: parent.verticalCenter
                name: "flag"
                size: 13
                color: UI.Style.accent
            }
            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                text: VoiceStyle.time(control.time)
                size: 12
                color: UI.Style.inkMuted
                font.features: { "tnum": 1 }
            }
        }
    }
}
