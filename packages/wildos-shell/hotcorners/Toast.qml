import QtQuick
import QtQuick.Window
import WildOS.UI as UI

// The short note a hot corner shows when it switches something on or off
// ("Keep awake on"), in that corner of the screen, for about a second and a
// half. It never takes the pointer or the keyboard.
Window {
    id: toast

    // KWin lets an internal window catch the pointer unless it carries this.
    readonly property bool outputOnly: true
    readonly property bool _q_showWithoutActivating: true

    readonly property int margin: 16

    flags: Qt.ToolTip | Qt.FramelessWindowHint | Qt.BypassWindowManagerHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput
    color: "transparent"
    width: pill.width
    height: pill.height

    function showAt(text, area, corner) {
        label.text = text;
        const right = corner === 1 || corner === 3;
        const bottom = corner === 2 || corner === 3;
        x = Math.round(right ? area.x + area.width - width - margin : area.x + margin);
        y = Math.round(bottom ? area.y + area.height - height - margin : area.y + margin);
        fade.stop();
        pill.opacity = 1;
        visible = true;
        fade.start();
    }

    Rectangle {
        id: pill

        width: label.implicitWidth + 32
        height: 40
        radius: UI.Style.radiusMd
        color: UI.Style.glassMenu
        border.width: 1
        border.color: UI.Style.borderMenu

        UI.Label {
            id: label
            anchors.centerIn: parent
            size: 13
            font.weight: UI.Style.weightSemiBold
        }
    }

    SequentialAnimation {
        id: fade
        PauseAnimation { duration: 1300 }
        NumberAnimation { target: pill; property: "opacity"; to: 0; duration: 200; easing.type: Easing.OutCubic }
        ScriptAction { script: toast.visible = false }
    }
}
