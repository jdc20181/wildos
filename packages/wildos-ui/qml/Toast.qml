import QtQuick
import QtQuick.Layouts

// A short note near the bottom of a window that fades on its own ("Link
// copied", "Firefox is installed"), with an optional icon and one action.
// From wildos-store Toast (icon and action) and wildos-files Toast (plain
// note): 52px with an icon or action, 40 without, radius 12, menu glass,
// 4 s, or 8 s when there is an action to reach. Toasts are for the app's
// own news; anything that matters after the window closes goes to a
// system notification instead.
Rectangle {
    id: toast

    property string text
    property string iconName
    property string themeIcon
    property string actionText
    property int duration: actionText.length > 0 ? 8000 : 4000

    signal actionClicked()

    function show(message, action) {
        text = message;
        actionText = action ?? "";
        opacity = 1;
        hideTimer.restart();
    }

    function hide() {
        opacity = 0;
    }

    readonly property bool rich: iconName.length > 0 || themeIcon.length > 0 || actionText.length > 0

    implicitWidth: Math.min(row.implicitWidth + 32, 560)
    implicitHeight: rich ? 52 : 40
    radius: Style.radiusMd
    color: Style.glassMenu
    border.width: 1
    border.color: Style.borderMenu
    opacity: 0
    visible: opacity > 0
    Behavior on opacity {
        NumberAnimation { duration: Style.motionNormal; easing.type: Easing.OutCubic }
    }
    // Rises into place from just below as it fades in, and sinks as it
    // fades out, so it reads as coming from the bottom edge.
    transform: Translate {
        y: (1 - toast.opacity) * Style.travelSmall
    }
    Accessible.role: Accessible.AlertMessage
    Accessible.name: text

    // Hovering keeps it up, as Windows notifications do.
    HoverHandler {
        id: hover
        onHoveredChanged: hovered ? hideTimer.stop() : hideTimer.restart()
    }

    Timer {
        id: hideTimer
        interval: toast.duration
        onTriggered: toast.opacity = 0
    }

    RowLayout {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        x: 16
        width: parent.width - 32
        spacing: 12

        Icon {
            visible: toast.iconName.length > 0 && toast.themeIcon.length === 0
            name: toast.iconName
            size: 18
            color: Style.accent
        }

        ThemeIcon {
            visible: toast.themeIcon.length > 0
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            name: toast.themeIcon
        }

        Label {
            Layout.fillWidth: true
            Layout.maximumWidth: 380
            text: toast.text
        }

        Button {
            visible: toast.actionText.length > 0
            size: "small"
            text: toast.actionText
            onClicked: {
                toast.actionClicked();
                toast.opacity = 0;
            }
        }
    }
}
