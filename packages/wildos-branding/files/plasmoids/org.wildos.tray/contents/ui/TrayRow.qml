pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// What sits on the taskbar: the arrow (only when something is behind it),
// the inline app icons, and the "safely remove" icon while a drive is in use.
RowLayout {
    id: row

    required property var tray

    spacing: 0
    Layout.minimumWidth: implicitWidth
    Layout.preferredWidth: implicitWidth
    Layout.maximumWidth: implicitWidth
    Layout.minimumHeight: 44

    HoverHandler {
        onPointChanged: Nav.keyboard = false
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) {
            Nav.keyboard = true;
        }
        event.accepted = false;
    }

    UI.IconButton {
        visible: row.tray.overflowCount > 0
        Layout.preferredWidth: 32
        Layout.preferredHeight: 36
        checked: row.tray.expanded && row.tray.mode === "overflow"
        tip: "Show hidden icons"
        iconName: "chevron-up"
        onClicked: row.tray.open("overflow")
    }

    Repeater {
        model: row.tray.items

        TrayIcon {
            required property int index
            visible: row.tray.inlineRows.indexOf(index) >= 0
            Layout.preferredWidth: 32
            Layout.preferredHeight: 44
            fillWidth: 32
            fillHeight: 36
        }
    }

    UI.IconButton {
        visible: row.tray.mountedDrives.count > 0
        Layout.preferredWidth: 32
        Layout.preferredHeight: 36
        checked: row.tray.expanded && row.tray.mode === "usb"
        tip: "Safely remove a drive"
        onClicked: row.tray.open("usb")

        // A USB plug with a tick, Windows' "safely remove hardware" mark,
        // which the shared icon set does not have.
        UI.Glyph {
            anchors.centerIn: parent
            width: 16
            height: 16
            path: "M8 3h6v6H8zM6 9h10v6a3 3 0 0 1-3 3h-4a3 3 0 0 1-3-3zM11 18v3M14 13.5l2 2 4-4"
        }
    }
}
