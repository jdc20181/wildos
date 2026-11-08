import QtQuick
import QtQuick.Templates as T

// A hover label for icon-only buttons and anything else that needs a name:
// one line of 12px text in the menu glass, 600ms after the pointer stops,
// below the control. From wildos-files Tip. Plasmoids use it in place of
// PlasmaCore.ToolTipArea, whose bold two-line Plasma tooltip is not the
// Windows look.
T.ToolTip {
    id: control

    delay: 600
    timeout: 6000
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? parent.height + 6 : 0
    margins: 6
    leftPadding: 10
    rightPadding: 10
    topPadding: 6
    bottomPadding: 6
    implicitWidth: Math.min(360, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    contentItem: Label {
        text: control.text
        size: 12
        wrapMode: Text.Wrap
        elide: Text.ElideNone
    }

    background: Rectangle {
        radius: Style.radiusXs
        color: Style.glassMenu
        border.width: 1
        border.color: Style.borderMenu
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Style.motionFast }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Math.round(Style.motionFast / 2) }
    }
}
