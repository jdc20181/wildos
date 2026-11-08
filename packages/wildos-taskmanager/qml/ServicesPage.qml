import QtQuick
import QtQuick.Templates as T
import WildOS.TaskManager
import WildOS.UI as UI

// Services: what the system and your account run in the background, by
// plain name, with whether each is running and how it starts. Start, Stop
// and Restart act on the selected one; a system service asks for an
// administrator's password first. "Show all" adds the parts of the system
// nobody normally needs to touch.
Item {
    id: page

    property string selectedKey: ""
    readonly property var selected: Services.services.find(s => s.key === selectedKey) || null

    signal message(string text)

    function act(action) {
        if (page.selected) {
            Services.act(page.selected.unit, page.selected.user, action);
        }
    }

    Connections {
        target: Services
        function onFailed(text) {
            page.message(text);
        }
    }

    PageHeader {
        id: header
        width: parent.width
        title: "Services"

        Row {
            height: 32
            spacing: 10

            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                text: "Show all"
                size: 12
            }

            UI.Toggle {
                anchors.verticalCenter: parent.verticalCenter
                checked: Services.showAll
                Accessible.name: "Show all services"
                onToggled: Services.showAll = checked
            }

            Item {
                width: 6
                height: 1
            }
        }

        HeaderButton {
            text: "Start"
            iconName: "play"
            enabled: page.selected !== null && !page.selected.running && !page.selected.masked
            onClicked: page.act("start")
        }

        HeaderButton {
            text: "Restart"
            iconName: "restart"
            enabled: page.selected !== null && page.selected.running
            onClicked: page.act("restart")
        }

        HeaderButton {
            kind: "primary"
            text: "Stop"
            iconName: "close"
            enabled: page.selected !== null && page.selected.running
            onClicked: page.act("stop")
        }
    }

    TableHeading {
        id: heading
        y: header.height
        width: parent.width
        columns: [
            { title: "Name", width: 0 },
            { title: "Service", width: 220 },
            { title: "Status", width: 110 },
            { title: "Starts", width: 130 },
            { title: "Runs for", width: 90 }
        ]
    }

    ListView {
        id: list
        y: heading.y + heading.height
        width: parent.width
        height: parent.height - y
        clip: true
        model: Services.services
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        T.ScrollBar.vertical: UI.ScrollBar {}

        delegate: TableRow {
            id: row
            required property var modelData
            width: list.width
            implicitHeight: 40
            selected: modelData.key === page.selectedKey
            Accessible.name: modelData.name + ", " + modelData.status
            onClicked: page.selectedKey = modelData.key
            onMenuRequested: at => {
                menu.entries = [
                    { text: "Start", icon: "play", enabled: !row.modelData.running && !row.modelData.masked, run: () => page.act("start") },
                    { text: "Stop", icon: "close", enabled: row.modelData.running, run: () => page.act("stop") },
                    { text: "Restart", icon: "restart", enabled: row.modelData.running, run: () => page.act("restart") }
                ];
                menu.popup(row, at.x, at.y);
            }

            UI.Label {
                width: row.width - 220 - 110 - 130 - 90
                height: row.height
                leftPadding: 16
                rightPadding: 12
                text: row.modelData.name
            }

            UI.Label {
                width: 220
                height: row.height
                leftPadding: 12
                rightPadding: 12
                text: row.modelData.unit
                size: 12
                color: UI.Style.inkMuted
            }

            UI.Label {
                width: 110
                height: row.height
                leftPadding: 12
                text: row.modelData.status
                size: 12
                color: row.modelData.status === "Failed" ? UI.Style.danger : UI.Style.inkMuted
            }

            UI.Label {
                width: 130
                height: row.height
                leftPadding: 12
                text: row.modelData.starts
                size: 12
                color: UI.Style.inkMuted
            }

            UI.Label {
                width: 90
                height: row.height
                leftPadding: 12
                text: row.modelData.owner
                size: 12
                color: UI.Style.inkMuted
            }
        }
    }

    UI.Label {
        visible: list.count === 0
        anchors.centerIn: list
        text: Services.search.length > 0 ? "No service matches “" + Services.search + "”." : "Wild OS could not read the list of services."
        color: UI.Style.inkMuted
    }

    UI.Menu {
        id: menu
    }
}
