import QtQuick
import WildOS.UI as UI

// The design's notification buttons: 32 px tall, 8 px corners, 12 px text.
// Primary is the accent fill; secondary a light fill with a hairline edge.
// The hit area reaches 44 px tall so it stays easy to tap.
CardButton {
    id: action

    property string text
    property bool primary: false

    implicitHeight: 44
    fillHeight: 32
    baseColor: primary ? UI.Style.accent : UI.Style.surfaceControl
    hoverColor: primary ? UI.Style.accentHover : UI.Style.surfaceHover
    // A filled button darkens while held rather than turning grey.
    pressedColor: primary ? UI.Style.accentPressed : UI.Style.surfacePressed
    accessibleName: text

    Rectangle {
        visible: !action.primary
        anchors.centerIn: parent
        width: action.width
        height: 32
        radius: 8
        color: "transparent"
        border.width: 1
        border.color: UI.Style.borderControl
    }

    UI.Label {
        anchors.centerIn: parent
        width: Math.min(implicitWidth, action.width - 16)
        horizontalAlignment: Text.AlignHCenter
        text: action.text
        size: 12
        font.weight: action.primary ? Font.DemiBold : Font.Medium
        color: action.primary ? UI.Style.textOnAccent : UI.Style.ink
    }
}
