import QtQuick
import WildOS.UI as UI

// The 8 px status dot. While Windows is starting or updating a ring around
// it pulses; with reduced motion the ring stays still, as drawn on the board.
Item {
    id: dot

    property string level: ""
    readonly property bool pulsing: level === "starting" || level === "updating"
    readonly property color colour: Tones.dot(level)

    implicitWidth: 8
    implicitHeight: 8

    Rectangle {
        id: ring
        anchors.centerIn: parent
        width: 16
        height: 16
        radius: 8
        color: "transparent"
        border.width: 1.5
        border.color: dot.colour
        opacity: 0.45
        visible: dot.pulsing

        SequentialAnimation {
            running: dot.pulsing && !UI.Style.reducedMotion && dot.visible
            loops: Animation.Infinite
            onStopped: {
                ring.scale = 1;
                ring.opacity = 0.45;
            }

            ParallelAnimation {
                NumberAnimation { target: ring; property: "scale"; from: 0.6; to: 1.15; duration: 1100; easing.type: Easing.OutCubic }
                NumberAnimation { target: ring; property: "opacity"; from: 0.6; to: 0; duration: 1100; easing.type: Easing.OutCubic }
            }
            PauseAnimation { duration: 300 }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: dot.colour
    }
}
