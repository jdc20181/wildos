import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// A category to filter by: the chip shape, accent when picked.
T.AbstractButton {
    id: pill

    property bool selected: false

    implicitWidth: label.implicitWidth + 28
    implicitHeight: Math.max(32, Math.ceil(label.implicitHeight) + 12)
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.RadioButton
    Accessible.name: text
    Accessible.checked: selected

    background: Rectangle {
        radius: height / 2
        color: pill.selected ? UI.Style.accentChip : pill.down ? UI.Style.surfacePressed : pill.hovered ? UI.Style.surfaceHover : Metrics.chip
        border.width: pill.selected ? 1 : 0
        border.color: UI.Style.accentBorder
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
        font.weight: Font.Medium
        color: pill.selected ? UI.Style.accentText : Metrics.inkBody
    }
}
