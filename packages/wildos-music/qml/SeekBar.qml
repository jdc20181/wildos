import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// The player bar's seek and volume bars (AppMusic.dc): a 4px track with a
// white fill and, for seeking, a 12px white knob. The bar follows `source`
// except while it is being dragged; `committed` reports where it was let go.
T.Slider {
    id: control

    property real source: 0
    property bool knob: true
    property color fill: MusicStyle.barFill
    property string accessibleName

    signal committed(real value)

    implicitHeight: 16
    implicitWidth: 420
    padding: 0
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    wheelEnabled: false
    Accessible.name: accessibleName
    Accessible.role: Accessible.Slider

    Binding on value {
        value: control.source
        when: !control.pressed
        restoreMode: Binding.RestoreNone
    }
    onPressedChanged: {
        if (!pressed) {
            committed(value);
        }
    }
    Keys.onLeftPressed: committed(Math.max(from, value - (to - from) / 20))
    Keys.onRightPressed: committed(Math.min(to, value + (to - from) / 20))

    background: Item {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 4

        Rectangle {
            anchors.fill: parent
            radius: 2
            color: MusicStyle.barTrack
        }
        Rectangle {
            width: Math.max(0, Math.min(parent.width, control.visualPosition * parent.width))
            height: parent.height
            radius: 2
            color: control.enabled ? control.fill : MusicStyle.barTrack
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * control.availableWidth - width / 2
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 12
        height: 12
        radius: 6
        visible: control.enabled && (control.knob || control.hovered || control.pressed || control.visualFocus)
        color: MusicStyle.barFill
        border.width: UI.Style.dark ? 0 : 1
        border.color: UI.Style.borderControl
        scale: control.pressed ? 1.15 : 1
        Behavior on scale {
            NumberAnimation { duration: UI.Style.motionFast }
        }

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: 6
        }
    }
}
