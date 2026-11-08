import QtQuick

// Drawn 2px outside its target, like the design's outline-offset.
Rectangle {
    property real targetRadius: Style.buttonRadius

    anchors.fill: parent
    anchors.margins: -4
    radius: targetRadius + 4
    color: "transparent"
    border.width: 2
    border.color: Style.accent
    z: 10
}
