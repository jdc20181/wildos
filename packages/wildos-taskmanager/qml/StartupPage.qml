import QtQuick
import QtQuick.Templates as T
import WildOS.TaskManager
import WildOS.UI as UI

// Startup apps: what starts when you sign in, each with its switch, as
// Windows lists them. The list and the switches are the same as Settings >
// Apps > Startup (both write the XDG autostart override), so a change in
// either shows in the other.
Item {
    id: page

    property string search
    property string selectedFile: ""
    readonly property var apps: {
        const needle = search.trim().toLowerCase();
        return StartupApps.apps.filter(a => needle.length === 0 || a.name.toLowerCase().includes(needle));
    }
    readonly property var selected: apps.find(a => a.file === selectedFile) || null

    PageHeader {
        id: header
        width: parent.width
        title: "Startup apps"

        HeaderButton {
            text: page.selected && page.selected.enabled ? "Disable" : "Enable"
            iconName: page.selected && page.selected.enabled ? "close" : "check"
            enabled: page.selected !== null
            onClicked: StartupApps.setEnabled(page.selected.file, !page.selected.enabled)
        }
    }

    TableHeading {
        id: heading
        y: header.height
        width: parent.width
        columns: [
            { title: "Name", width: 0 },
            { title: "Status", width: 120 },
            { title: "", width: 84 }
        ]
    }

    ListView {
        id: list
        y: heading.y + heading.height
        width: parent.width
        height: parent.height - y
        clip: true
        model: page.apps
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: UI.ScrollBar {}

        delegate: TableRow {
            id: row
            required property var modelData
            width: list.width
            selected: modelData.file === page.selectedFile
            Accessible.name: modelData.name + ", " + (modelData.enabled ? "Enabled" : "Disabled")
            onClicked: page.selectedFile = modelData.file

            Item {
                width: row.width - 120 - 84
                height: row.height

                AppIcon {
                    x: 16
                    width: 20
                    height: 20
                    anchors.verticalCenter: parent.verticalCenter
                    icon: row.modelData.icon
                }

                Column {
                    x: 16 + 20 + 12
                    width: parent.width - x - 12
                    anchors.verticalCenter: parent.verticalCenter

                    UI.Label {
                        width: parent.width
                        text: row.modelData.name
                    }

                    UI.Label {
                        visible: text.length > 0
                        width: parent.width
                        text: row.modelData.description
                        size: 11
                        color: UI.Style.inkMuted
                    }
                }
            }

            UI.Label {
                width: 120
                height: row.height
                leftPadding: 12
                text: row.modelData.enabled ? "Enabled" : "Disabled"
                size: 12
                color: UI.Style.inkMuted
            }

            Item {
                width: 84
                height: row.height

                UI.Toggle {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    checked: row.modelData.enabled
                    Accessible.name: "Start " + row.modelData.name + " when you sign in"
                    onToggled: StartupApps.setEnabled(row.modelData.file, checked)
                }
            }
        }
    }

    UI.Label {
        visible: list.count === 0
        anchors.centerIn: list
        width: Math.min(420, list.width - 48)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: page.search.length > 0 ? "No startup app matches “" + page.search + "”." : "No apps start when you sign in. Apps you set to start with Wild OS show here."
        color: UI.Style.inkMuted
    }
}
