import QtQuick

// One key of a keyboard shortcut, as the design draws a <kbd> (the
// Accessibility shortcuts, SettingsAccessibility.dc): 22px tall and at
// least as wide, 6px either side of an 11px medium label, a 5px corner and
// a hairline that is a pixel heavier along the bottom, like a keycap. The
// label grows with the text size; the cap grows with it.
Rectangle {
    id: chip

    property string text

    implicitWidth: Math.max(implicitHeight, label.implicitWidth + 12)
    implicitHeight: Math.max(22, label.implicitHeight + 6)
    radius: 5
    color: Style.surfaceControl
    border.width: 1
    border.color: Style.tone(0.20)
    Accessible.ignored: true

    Rectangle {
        x: 3
        y: parent.height - 2
        width: parent.width - 6
        height: 1
        color: chip.border.color
    }

    Label {
        id: label
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -0.5
        text: chip.text
        size: 11
        font.weight: Style.weightMedium
        color: Style.ink
    }
}
