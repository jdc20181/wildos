import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// The accent New note button (AppNotes.dc): 32px, 16 in, radius 6, a pen on
// paper 8px before 13px semibold words.
T.Button {
    id: control

    implicitWidth: row.implicitWidth + 32
    implicitHeight: 32
    padding: 0
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    text: qsTr("New note")
    Accessible.name: text
    Accessible.role: Accessible.Button

    background: Rectangle {
        radius: UI.Style.radiusXs
        color: control.down ? UI.Style.accentPressed : control.hovered ? UI.Style.accentHover : UI.Style.accent
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
            spacing: 8

            NotesIcon {
                anchors.verticalCenter: parent.verticalCenter
                name: "new-note"
                size: 15
                color: UI.Style.textOnAccent
            }
            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                text: control.text
                size: 13
                font.weight: UI.Style.weightSemiBold
                color: UI.Style.textOnAccent
            }
        }
    }
}
