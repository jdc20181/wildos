import QtQuick
import QtQuick.Templates as T

// One colour of a set to choose from, such as the pointer colours in
// Accessibility (SettingsAccessibility.dc): a 22px disc with a hairline, so
// white reads on light glass and black on dark, and the chosen one ringed
// in the accent 2px clear of the disc. Click it to choose it.
T.AbstractButton {
    id: swatch

    property color colour
    property string name
    property bool selected: false
    property int size: 22

    implicitWidth: size
    implicitHeight: size
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.RadioButton
    Accessible.name: name
    Accessible.checked: selected

    background: Rectangle {
        radius: width / 2
        color: swatch.down ? Qt.darker(swatch.colour, 1.06) : swatch.colour
        border.width: 1
        border.color: Style.tone(0.30)

        Rectangle {
            visible: swatch.selected
            anchors.fill: parent
            anchors.margins: -4
            radius: width / 2
            color: "transparent"
            border.width: 2
            border.color: Style.accent
        }

        Rectangle {
            visible: swatch.hovered && !swatch.selected
            anchors.fill: parent
            anchors.margins: -3
            radius: width / 2
            color: "transparent"
            border.width: 1
            border.color: Style.tone(0.30)
        }

        FocusRing {
            visible: swatch.visualFocus
            anchors.margins: swatch.selected ? -7 : -4
            targetRadius: width / 2
        }
    }

    Tooltip {
        text: swatch.name
        visible: swatch.hovered && swatch.name.length > 0
    }
}
