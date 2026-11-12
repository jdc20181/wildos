import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// The volume bar beside the speaker (AppVideos.dc): 80 by 4, a white fill
// on a faint track, with a knob only under the pointer.
T.Slider {
    id: control

    implicitWidth: 80
    implicitHeight: 18
    padding: 0
    from: 0
    to: 100
    stepSize: 1
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    wheelEnabled: true
    Accessible.name: qsTr("Volume")

    background: Item {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 4

        Rectangle {
            anchors.fill: parent
            radius: 2
            color: VideosStyle.overPictureTrack
        }
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: "#ffffff"
            opacity: Player.muted ? 0.4 : 1
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * control.availableWidth - width / 2
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 12
        height: 12
        radius: 6
        color: "#ffffff"
        visible: control.hovered || control.pressed || control.visualFocus

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: 6
        }
    }
}
