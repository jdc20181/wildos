import QtQuick
import WildOS.UI as UI

// The playing song's mark in place of its number: the board's six level
// bars, moving while it plays and still while it is paused.
Item {
    id: bars

    property color color: MusicStyle.playingInk
    property bool running: false
    property real size: 15

    implicitWidth: size
    implicitHeight: size

    // The board's bar heights on its 24px grid, tallest in the middle.
    readonly property var rest: [3, 11, 18, 7, 13, 3]

    Repeater {
        model: 6
        delegate: Rectangle {
            id: bar
            required property int index
            readonly property real unit: bars.size / 24
            property real level: bars.rest[index]

            x: (2 + index * 4) * unit - width / 2
            width: Math.max(1.5, 2 * unit)
            height: Math.max(width, level * unit)
            y: (bars.height - height) / 2
            radius: width / 2
            color: bars.color

            SequentialAnimation on level {
                running: bars.running && !UI.Style.reducedMotion
                loops: Animation.Infinite
                alwaysRunToEnd: false
                NumberAnimation { to: [9, 16, 6, 18, 8, 12][bar.index]; duration: 260 + bar.index * 37; easing.type: Easing.InOutSine }
                NumberAnimation { to: [4, 7, 18, 10, 17, 6][bar.index]; duration: 300 + bar.index * 23; easing.type: Easing.InOutSine }
                NumberAnimation { to: bars.rest[bar.index]; duration: 240 + bar.index * 31; easing.type: Easing.InOutSine }
            }
        }
    }
}
