import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// A filter at the top of a category: a 32px pill, filled with the accent
// when picked.
T.AbstractButton {
    id: pill

    property bool selected: false

    implicitWidth: label.implicitWidth + 30
    implicitHeight: Math.max(32, Math.ceil(label.implicitHeight) + 12)
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.RadioButton
    Accessible.name: text
    Accessible.checked: selected

    background: Rectangle {
        radius: height / 2
        color: pill.selected ? (pill.hovered ? UI.Style.accentHover : UI.Style.accent)
            : pill.hovered || pill.down ? Metrics.buttonFill : Metrics.pill
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: pill.visualFocus
            targetRadius: parent.radius
        }
    }

    contentItem: UI.Label {
        id: label
        horizontalAlignment: Text.AlignHCenter
        text: pill.text
        size: 13.5
        font.weight: pill.selected ? Font.DemiBold : Font.Medium
        color: pill.selected ? UI.Style.textOnAccent : UI.Style.ink
    }
}
