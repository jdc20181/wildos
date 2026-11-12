import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// The taskbar button: a 16 px shield in a 32 px cell with a 32 by 36 hover,
// the same as the tray's icons and the Updates button, so the gaps between
// them all match. No label. The
// shield carries the state: accent with a tick when all is well, sand with
// "!" when something needs doing, and plain while the first check runs (so
// login never flashes a warning) or while updates have not been checked.
MouseArea {
    id: chip

    required property var security

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
    Accessible.name: "Security: " + security.chipText.toLowerCase()
    Accessible.description: security.toolTipSubText

    // Remembering the state at press time stops a click that closes the popup
    // (it loses focus first) from opening it again straight away.
    onPressed: wasExpanded = security.expanded
    onClicked: security.expanded = !wasExpanded

    Rectangle {
        anchors.centerIn: parent
        width: 32
        height: 36
        radius: 8
        color: chip.pressed ? UI.Style.surfacePressed : chip.security.expanded ? UI.Style.surfaceSelected : chip.containsMouse ? UI.Style.surfaceHover : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }
    }

    ShieldGlyph {
        anchors.centerIn: parent
        width: 16
        height: 16
        mark: chip.security.level === "protected" ? "tick"
            : chip.security.level === "attention" ? "bang" : ""
        color: chip.security.level === "attention" ? UI.Style.warning : UI.Style.accent
    }
}
