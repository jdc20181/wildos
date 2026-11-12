import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// The taskbar button: a 16 px update icon in a 32 px cell with a 32 by 36
// hover, the same as the tray's icons and the security button beside it. A dot on the
// icon when updates are ready to install (sand when security fixes are
// among them), the icon turning while something checks or installs, and
// the warning sign when the last check failed.
MouseArea {
    id: button

    required property var updates

    property bool wasExpanded: false

    // The panel puts about 5 px between widgets, where the tray's own icons
    // sit edge to edge. 27 px here keeps the icons 32 px apart, the same as
    // Wi-Fi, volume and battery; the hover stays the tray's full 32 px.
    Layout.minimumWidth: 27
    Layout.preferredWidth: Layout.minimumWidth
    Layout.maximumWidth: Layout.minimumWidth
    Layout.minimumHeight: 44

    hoverEnabled: true
    Accessible.role: Accessible.Button
    Accessible.name: "Updates: " + updates.summary.toLowerCase()
    Accessible.description: updates.toolTipSubText

    // Remembering the state at press time stops a click that closes the popup
    // (it loses focus first) from opening it again straight away.
    onPressed: wasExpanded = updates.expanded
    onClicked: updates.expanded = !wasExpanded

    Rectangle {
        anchors.centerIn: parent
        width: 32
        height: 36
        radius: 8
        color: button.pressed ? UI.Style.surfacePressed : button.updates.expanded ? UI.Style.surfaceSelected : button.containsMouse ? UI.Style.surfaceHover : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }
    }

    UI.Icon {
        id: icon
        anchors.centerIn: parent
        name: button.updates.failed ? "warning" : "update"
        size: 16
        strokeWidth: 2
        color: button.updates.failed ? UI.Style.warning : UI.Style.iconInk

        // A plain number animation, not an animator, so the angle is known
        // when the work ends and the turn can finish instead of snapping back
        // to upright. Easing out over twice the time the spin would take
        // leaves at the spin's own speed and slows to rest.
        NumberAnimation {
            id: spin
            target: icon
            property: "rotation"
            from: 0
            to: 360
            duration: 1200
            loops: Animation.Infinite
            running: button.updates.busy
        }
        NumberAnimation {
            id: settle
            target: icon
            property: "rotation"
            to: 360
            easing.type: Easing.OutQuad
            onFinished: icon.rotation = 0
        }
        Connections {
            target: button.updates
            function onBusyChanged() {
                if (button.updates.busy) {
                    settle.stop();
                } else if (icon.rotation > 0) {
                    settle.duration = Math.max(200, 2 * (360 - icon.rotation) / 360 * 1200);
                    settle.start();
                }
            }
        }
    }

    // On the arrow's end, top right of the icon, where it reads as a badge
    // without covering the arrow.
    Rectangle {
        visible: button.updates.ready
        x: icon.x + icon.width - width + 1
        y: icon.y - 1
        width: 8
        height: 8
        radius: 4
        color: button.updates.securityCount > 0 ? UI.Style.warning : UI.Style.accent
    }
}
