import QtQuick
import QtQuick.Templates as T
import WildOS.TaskManager
import WildOS.UI as UI

// Windows apps: every app the App Bridge installed, whether its Windows
// mode session is running, suspended (idle, frozen so it costs nothing) or
// not running, and what it uses now. Open starts it as Start does, End
// task asks its session to close, and Settings opens Settings > Windows
// apps, where its permissions and removal live.
Item {
    id: page

    property string search
    property string selectedId: ""
    readonly property var apps: {
        const needle = search.trim().toLowerCase();
        return WindowsApps.apps.filter(a => needle.length === 0 || a.name.toLowerCase().includes(needle) || a.publisher.toLowerCase().includes(needle));
    }
    readonly property var selected: apps.find(a => a.id === selectedId) || null

    PageHeader {
        id: header
        width: parent.width
        title: "Windows apps"

        HeaderButton {
            text: "Settings"
            iconName: "options"
            onClicked: WindowsApps.openSettings()
        }

        HeaderButton {
            text: "Open"
            iconName: "open"
            enabled: page.selected !== null
            onClicked: WindowsApps.open(page.selected.id)
        }

        HeaderButton {
            kind: "primary"
            text: "End task"
            iconName: "close"
            enabled: page.selected !== null && page.selected.running && page.selected.canEnd
        }
    }

    TableHeading {
        id: heading
        y: header.height
        width: parent.width
        columns: [
            { title: "Name", width: 0 },
            { title: "Publisher", width: 200 },
            { title: "Status", width: 120 },
            { title: "CPU", width: 92, right: true },
            { title: "Memory", width: 92, right: true }
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
            selected: modelData.id === page.selectedId
            Accessible.name: modelData.name + ", " + modelData.status
            onClicked: page.selectedId = modelData.id
            onDoubleClicked: WindowsApps.open(modelData.id)
            onMenuRequested: at => {
                menu.entries = [
                    { text: "Open", icon: "open", run: () => WindowsApps.open(row.modelData.id) },
                    { text: "End task", icon: "close", enabled: row.modelData.running && row.modelData.canEnd, run: () => {} },
                    { separator: true },
                    { text: "Windows app settings", icon: "options", run: () => WindowsApps.openSettings() }
                ];
                menu.popup(row, at.x, at.y);
            }

            Item {
                width: row.width - 200 - 120 - 2 * 92
                height: row.height

                AppIcon {
                    x: 16
                    width: 20
                    height: 20
                    anchors.verticalCenter: parent.verticalCenter
                    icon: row.modelData.icon
                }

                UI.Label {
                    x: 16 + 20 + 12
                    width: parent.width - x - 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.name
                }
            }

            UI.Label {
                width: 200
                height: row.height
                leftPadding: 12
                rightPadding: 12
                text: row.modelData.publisher
                size: 12
                color: UI.Style.inkMuted
            }

            UI.Label {
                width: 120
                height: row.height
                leftPadding: 12
                text: row.modelData.status
                size: 12
                color: UI.Style.inkMuted
            }

            Rectangle {
                width: 92
                height: row.height - 1
                color: Colours.heatColour(Colours.heat("cpu", row.modelData.cpu, 1))

                UI.Label {
                    anchors.fill: parent
                    anchors.rightMargin: 12
                    horizontalAlignment: Text.AlignRight
                    text: row.modelData.running ? Format.cpu(row.modelData.cpu) : ""
                    size: 12
                    font.features: { "tnum": 1 }
                }
            }

            Rectangle {
                width: 92
                height: row.height - 1
                color: Colours.heatColour(Colours.heat("memory", row.modelData.memory, SystemStats.memoryTotal))

                UI.Label {
                    anchors.fill: parent
                    anchors.rightMargin: 12
                    horizontalAlignment: Text.AlignRight
                    text: row.modelData.running ? Format.memory(row.modelData.memory) : ""
                    size: 12
                    font.features: { "tnum": 1 }
                }
            }
        }
    }

    UI.Label {
        visible: list.count === 0
        anchors.centerIn: list
        width: Math.min(440, list.width - 48)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: page.search.length > 0 ? "No Windows app matches “" + page.search + "”." : "No Windows apps yet. Open a Windows installer (.exe or .msi) in Files to add one."
        color: UI.Style.inkMuted
    }

    UI.Menu {
        id: menu
    }
}
