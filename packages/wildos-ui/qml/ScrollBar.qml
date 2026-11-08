import QtQuick
import QtQuick.Templates as T

// A slim bar that only shows while scrolling or hovered, like Windows 11:
// a 3px thumb that widens to 6px under the pointer. From wildos-files and
// wildos-store ThinScrollBar (the plasmoid copies stay 4px and never
// widen). Attach it as `ScrollBar.vertical: UI.ScrollBar {}`.
T.ScrollBar {
    id: control

    implicitWidth: orientation === Qt.Vertical ? 10 : 100
    implicitHeight: orientation === Qt.Horizontal ? 10 : 100
    padding: 2
    minimumSize: 0.08
    policy: T.ScrollBar.AsNeeded
    visible: policy === T.ScrollBar.AlwaysOn || size < 1

    contentItem: Rectangle {
        readonly property bool wide: control.hovered || control.pressed
        implicitWidth: control.orientation === Qt.Vertical ? (wide ? 6 : 3) : 100
        implicitHeight: control.orientation === Qt.Horizontal ? (wide ? 6 : 3) : 100
        radius: Math.min(width, height) / 2
        color: control.pressed ? Style.scrollThumbPressed : Style.scrollThumb
        opacity: control.policy === T.ScrollBar.AlwaysOn || control.active || control.hovered ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: Style.motionNormal }
        }
        Behavior on implicitWidth {
            NumberAnimation { duration: Style.motionFast }
        }
        Behavior on implicitHeight {
            NumberAnimation { duration: Style.motionFast }
        }
    }
}
