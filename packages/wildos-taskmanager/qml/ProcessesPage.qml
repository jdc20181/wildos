import QtQuick
import QtQuick.Templates as T
import WildOS.TaskManager
import WildOS.UI as UI

// Processes (TaskManager.dc): "Run new task" and "End task" in the header,
// then the table, its column headings carrying the whole computer's use
// (CPU, memory, the Wild OS disk's active time, the network's share of the
// connection), grouped into Apps, Background processes and Wild OS
// processes. Select a row and press End task or Delete; right-click for
// the rest.
Item {
    id: page

    property string selectedKey: ""
    // Read again whenever the list updates.
    readonly property var selected: Processes.processCount >= 0 && selectedKey.length > 0 ? Processes.entry(selectedKey) : ({})
    readonly property bool canEnd: selected.key !== undefined && selected.canEnd === true

    signal message(string text)
    signal runTask()

    function end(key, tree) {
        const info = Processes.entry(key);
        if (!info.key || !info.canEnd) {
            return;
        }
        // The system's own processes can take part of Wild OS with them.
        if (info.group === 2) {
            confirm.key = key;
            confirm.tree = tree;
            confirm.title = "End " + info.name + "?";
            confirm.open();
            return;
        }
        page.message(tree ? Processes.endTree(key) : Processes.endTask(key));
    }

    function showMenu(key, item, at) {
        const info = Processes.entry(key);
        if (!info.key) {
            return;
        }
        const entries = [];
        if (info.expandable) {
            entries.push({ text: info.expanded ? "Collapse" : "Expand", run: () => Processes.toggle(key) });
            entries.push({ separator: true });
        }
        entries.push({ text: info.endLabel, icon: info.endLabel === "Restart" ? "restart" : "close", enabled: info.canEnd, run: () => page.end(key, false) });
        if (!info.winMode) {
            entries.push({ text: "End process tree", enabled: info.canEnd, run: () => page.end(key, true) });
        }
        if (!info.winMode && info.single) {
            const levels = [["High", -10], ["Above normal", -5], ["Normal", 0], ["Below normal", 5], ["Low", 19]];
            entries.push({ separator: true });
            entries.push({
                text: "Set priority",
                submenu: levels.map(l => ({ text: l[0], run: () => page.message(Processes.setPriority(key, l[1])) }))
            });
            entries.push({ text: "Open file location", icon: "open-folder", run: () => Processes.openFileLocation(key) });
        }
        if (info.winMode) {
            entries.push({ separator: true });
            entries.push({ text: "Windows app settings", icon: "options", run: () => Prefs.openSettings("windowsapps") });
        }
        menu.entries = entries;
        menu.popup(item, at.x, at.y);
    }

    Keys.onDeletePressed: if (page.canEnd) page.end(page.selectedKey, false)

    PageHeader {
        id: header
        width: parent.width
        title: "Processes"

        HeaderButton {
            text: "Run new task"
            iconName: "plus"
            onClicked: page.runTask()
        }

        HeaderButton {
            kind: "primary"
            text: page.selected.endLabel || "End task"
            iconName: page.selected.endLabel === "Restart" ? "restart" : "close"
            enabled: page.canEnd
            onClicked: page.end(page.selectedKey, false)
        }
    }

    Item {
        id: columns
        y: header.height
        width: parent.width
        height: 53

        ColumnHeader {
            width: parent.width - 120 - 4 * 92
            height: parent.height - 1
            label: "Name"
            alignRight: false
            sorted: Processes.sortColumn === "name"
            descending: Processes.sortDescending
            onClicked: Processes.sortBy("name")

            // The design's Name heading sits on the middle line on its own.
            contentItem: Item {
                UI.Label {
                    x: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Name"
                    size: 12
                    color: UI.Style.inkAlpha(0.7, 0.64)
                }
                UI.Icon {
                    visible: Processes.sortColumn === "name" && Processes.sortDescending
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 3
                    name: Processes.sortDescending ? "chevron-down" : "chevron-up"
                    size: 10
                    strokeWidth: 2
                    color: UI.Style.iconMuted
                }
            }
        }

        Row {
            anchors.right: parent.right
            height: parent.height

            ColumnHeader {
                width: 120
                height: parent.height - 1
                label: "Status"
                alignRight: false
                sorted: Processes.sortColumn === "status"
                descending: Processes.sortDescending
                onClicked: Processes.sortBy("status")
            }

            Repeater {
                model: [
                    { id: "cpu", label: "CPU", total: () => Format.percent(SystemStats.cpuUsage) },
                    { id: "memory", label: "Memory", total: () => Format.percent(SystemStats.memoryPercent) },
                    { id: "disk", label: "Disk", total: () => Format.percent(SystemStats.diskPercent) },
                    { id: "network", label: "Network", total: () => Format.percent(SystemStats.networkPercent) }
                ]

                ColumnHeader {
                    required property var modelData
                    width: 92
                    height: parent.height - 1
                    label: modelData.label
                    total: SystemStats.revision >= 0 ? modelData.total() : ""
                    sorted: Processes.sortColumn === modelData.id
                    descending: Processes.sortDescending
                    onClicked: Processes.sortBy(modelData.id)
                }
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: UI.Style.tone(0.08)
        }
    }

    ListView {
        id: list
        y: columns.y + columns.height
        width: parent.width
        height: parent.height - y
        clip: true
        model: Processes
        boundsBehavior: Flickable.StopAtBounds
        focus: true
        keyNavigationEnabled: false
        reuseItems: true
        T.ScrollBar.vertical: UI.ScrollBar {}

        delegate: ProcessRow {
            width: list.width
            selected: key === page.selectedKey
            memoryTotal: SystemStats.memoryTotal
            onChosen: k => {
                page.selectedKey = k;
                list.forceActiveFocus();
            }
            onExpandToggled: k => Processes.toggle(k)
            onMenuRequested: (k, at) => page.showMenu(k, this, at)
        }

        function step(delta) {
            let i = Processes.indexOf(page.selectedKey) + delta;
            while (i >= 0 && i < count && Processes.isHeading(i)) {
                i += delta;
            }
            if (i < 0 || i >= count) {
                return;
            }
            page.selectedKey = Processes.keyAt(i);
            positionViewAtIndex(i, ListView.Contain);
        }

        Keys.onUpPressed: step(-1)
        Keys.onDownPressed: step(1)
        Keys.onRightPressed: if (page.selected.expandable) Processes.setExpanded(page.selectedKey, true)
        Keys.onLeftPressed: if (page.selected.expandable) Processes.setExpanded(page.selectedKey, false)
        Keys.onReturnPressed: if (page.selected.expandable) Processes.toggle(page.selectedKey)
        Keys.onMenuPressed: {
            const i = Processes.indexOf(page.selectedKey);
            const item = itemAtIndex(i);
            if (item) {
                page.showMenu(page.selectedKey, item, Qt.point(40, item.height));
            }
        }
    }

    UI.Label {
        visible: Processes.ready && list.count === 0
        anchors.centerIn: list
        text: Processes.search.length > 0 ? "Nothing running matches “" + Processes.search + "”." : ""
        color: UI.Style.inkMuted
    }

    UI.Menu {
        id: menu
    }

    UI.Dialog {
        id: confirm
        property string key
        property bool tree: false
        text: "Wild OS uses this to run. Ending it can make part of the system stop working until you restart."

        buttons: [
            UI.Button {
                kind: "primary"
                text: "End process"
                onClicked: {
                    confirm.close();
                    page.message(confirm.tree ? Processes.endTree(confirm.key) : Processes.endTask(confirm.key));
                }
            },
            UI.Button {
                text: "Cancel"
                onClicked: confirm.close()
            }
        ]
    }
}
