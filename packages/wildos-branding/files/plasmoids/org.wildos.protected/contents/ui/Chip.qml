import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// The taskbar chip from the design: an 18 px shield and 13 px label in a
// 44 px button with 12 px padding. Accent shield with a tick when all is
// well, a sand shield with "!" when something needs doing, and a plain
// shield while the first check runs so login never flashes a warning.
MouseArea {
    id: chip

    required property var security

    property bool wasExpanded: false

    Layout.minimumWidth: row.implicitWidth + 24
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
        width: parent.width
        height: 44
        radius: 12
        color: chip.pressed ? UI.Style.surfacePressed : chip.security.expanded ? UI.Style.surfaceSelected : chip.containsMouse ? UI.Style.surfaceHover : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }
    }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 8

        ShieldGlyph {
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            mark: chip.security.level === "protected" ? "tick"
                : chip.security.level === "attention" ? "bang" : ""
            color: chip.security.level === "attention" ? UI.Style.warning : UI.Style.accent
        }

        UI.Label {
            text: chip.security.chipText
        }
    }
}
