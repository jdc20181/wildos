import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// The boards' links ("See all", "Open Settings", the way back): words in
// the accent at medium weight, with an optional chevron before them.
T.AbstractButton {
    id: control

    property real size: 14
    property string iconName

    implicitWidth: row.implicitWidth + 8
    implicitHeight: row.implicitHeight + 6
    // So a heading beside it can line up with its words.
    baselineOffset: (height - row.height) / 2 + (row.height - label.height) / 2 + label.baselineOffset
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.name: text
    Accessible.role: Accessible.Link

    background: Item {
        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: UI.Style.radiusXs
        }
    }

    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 4
            opacity: control.down ? 0.75 : 1

            UI.Icon {
                visible: control.iconName.length > 0
                anchors.verticalCenter: parent.verticalCenter
                name: control.iconName
                size: 16
                strokeWidth: 2
                color: UI.Style.accent
            }

            UI.Label {
                id: label
                anchors.verticalCenter: parent.verticalCenter
                text: control.text
                size: control.size
                font.weight: UI.Style.weightMedium
                font.underline: control.hovered
                color: UI.Style.accent
            }
        }
    }
}
