import QtQuick

// The design's focus outline (.wos-btn:focus-visible): 2px accent, 2px clear
// of the control. Show it only for keyboard focus (visualFocus), never after
// a click.
Rectangle {
    property real targetRadius: Style.radiusSm

    anchors.fill: parent
    anchors.margins: -4
    radius: targetRadius + 4
    color: "transparent"
    border.width: 2
    border.color: Style.accent
}
