import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// The clock button from the design's taskbar: the time over the date, right
// aligned, in a 44 px tall button with 12 px padding. A small moon shows when
// do not disturb is on, so muted notifications are never a surprise.
MouseArea {
    id: button

    required property var clock

    property bool wasExpanded: false

    Layout.minimumWidth: row.implicitWidth + 24
    Layout.preferredWidth: Layout.minimumWidth
    Layout.maximumWidth: Layout.minimumWidth
    Layout.minimumHeight: 44

    hoverEnabled: true
    Accessible.role: Accessible.Button
    Accessible.name: "Date, time and notifications"
    Accessible.description: clock.toolTipMainText + ", " + time.text
        + (clock.toolTipSubText.length > 0 ? ". " + clock.toolTipSubText.replace(/\n/g, ". ") : "")

    // Remembering the state at press time stops a click that closes the popup
    // (it loses focus first) from opening it again straight away.
    onPressed: wasExpanded = clock.expanded
    onClicked: clock.expanded = !wasExpanded

    Rectangle {
        anchors.centerIn: parent
        width: parent.width
        height: 44
        radius: 12
        color: button.pressed ? UI.Style.surfacePressed : button.clock.expanded ? UI.Style.surfaceSelected : button.containsMouse ? UI.Style.surfaceHover : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }
    }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 8

        UI.Glyph {
            visible: button.clock.doNotDisturb
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            path: "M20 14.5A8.5 8.5 0 0 1 9.5 4 8.5 8.5 0 1 0 20 14.5z"
            color: UI.Style.inkStatus
        }

        ColumnLayout {
            spacing: 1

            UI.Label {
                id: time
                Layout.alignment: Qt.AlignRight
                text: Qt.formatTime(button.clock.now, "HH:mm")
                font.weight: Font.Medium
                font.features: { "tnum": 1 }
            }

            UI.Label {
                Layout.alignment: Qt.AlignRight
                text: Qt.formatDate(button.clock.now, "dd/MM/yyyy")
                size: 12
                font.features: { "tnum": 1 }
                color: UI.Style.inkStatus
            }
        }
    }
}
