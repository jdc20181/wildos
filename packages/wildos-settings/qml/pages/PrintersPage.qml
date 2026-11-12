import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Printers and scanners, after Windows 11: "Add a printer or scanner"
// searches the network and lists what it finds, each with Add device, with
// a way in by address for a printer that does not announce itself; below,
// the printers already added. Opening one shows its own page (Set as
// default, Print a test page, Remove), which stacks in the breadcrumb the
// way Windows does.
PageScroll {
    id: page

    property string selected
    readonly property var printer: {
        for (const p of Printers.printers) {
            if (p.name === selected) {
                return p;
            }
        }
        return null;
    }
    property bool adding: false
    property string addingUri

    readonly property int depth: selected.length > 0 ? 2 : 1
    readonly property var subTitles: printer ? [printer.info] : []

    function back() {
        if (selected.length > 0) {
            selected = "";
            return true;
        }
        return false;
    }

    function statusOf(p) {
        return (p.isDefault ? "Default, " + p.status.charAt(0).toLowerCase() + p.status.slice(1) : p.status)
            + (p.jobs > 0 ? ", " + Format.plural(p.jobs, "document", "documents") + " waiting" : "");
    }

    Component.onDestruction: Printers.stopSearch()

    // Printers change state on their own (paper runs out, a job finishes).
    Timer {
        interval: 4000
        running: Printers.available && page.visible
        repeat: true
        onTriggered: Printers.reload()
    }

    Connections {
        target: Printers
        function onDone(title, message) {
            notice.show(title, message, false);
            if (title === "Printer added") {
                page.adding = false;
            }
            if (page.selected.length > 0 && page.printer === null) {
                page.selected = "";
            }
        }
        function onFailed(title, reason) {
            notice.show(title, reason, true);
        }
    }

    Notice {
        id: notice
    }

    StatusCard {
        visible: !Printers.available
        iconName: "printer"
        warning: true
        title: "Printing isn't running"
        description: "The service that talks to printers is not running on this computer, so printers can't be added or used."
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: page.selected.length === 0 && Printers.available
        spacing: Metrics.sectionGap

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "printer"
                    iconTile: true
                    minHeight: 68
                    title: "Add a printer or scanner"
                    description: Printers.searching ? "Looking for printers on your network" : ""

                    UI.Spinner {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: Printers.searching
                    }

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Printers.searching ? "Stop" : page.adding ? "Search again" : "Add device"
                        kind: page.adding ? "secondary" : "primary"
                        enabled: Printers.busy !== "add"
                        onClicked: {
                            if (Printers.searching) {
                                Printers.stopSearch();
                            } else {
                                page.adding = true;
                                Printers.search();
                            }
                        }
                    }
                }

                Repeater {
                    model: page.adding ? Printers.found : []

                    UI.SettingRow {
                        id: foundRow
                        required property var modelData
                        iconName: "printer"
                        title: modelData.name
                        description: [modelData.model, modelData.location].filter(t => t && t.length > 0 && t !== modelData.name).join(", ")

                        UI.Button {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Add device"
                            busy: Printers.busy === "add" && page.addingUri === foundRow.modelData.uri
                            enabled: Printers.busy.length === 0
                            onClicked: {
                                page.addingUri = foundRow.modelData.uri;
                                Printers.add(foundRow.modelData.uri, foundRow.modelData.name);
                            }
                        }
                    }
                }

                UI.SettingRow {
                    visible: page.adding && !Printers.searching && Printers.found.length === 0
                    title: "No new printers found"
                    description: "Check the printer is on and joined to the same network as this computer."
                }

                UI.SettingRow {
                    visible: page.adding
                    title: "The printer that I want isn't listed"
                    description: "Add it by its network name or IP address"

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Add manually"
                        enabled: Printers.busy.length === 0
                        onClicked: addressDialog.open()
                    }
                }
            }
        }

        UI.Card {
            Layout.fillWidth: true
            visible: Printers.printers.length > 0

            Column {
                width: parent.width

                Repeater {
                    model: Printers.printers

                    UI.SettingRow {
                        required property var modelData
                        iconName: "printer"
                        title: modelData.info
                        description: page.statusOf(modelData)
                        descriptionColor: modelData.problem ? UI.Style.warning : UI.Style.inkMuted
                        chevron: true
                        onClicked: page.selected = modelData.name
                    }
                }
            }
        }

        UI.Label {
            Layout.fillWidth: true
            visible: Printers.printers.length === 0
            text: "No printers yet. Printers that work over the network without a driver, which is most made since 2014, can be added above."
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: Pages.exists("printersadvanced")
            spacing: 10

            SectionTitle {
                text: "Related settings"
            }

            UI.Card {
                Layout.fillWidth: true

                Column {
                    width: parent.width

                    UI.SettingRow {
                        iconName: "cog"
                        title: "More printer settings"
                        description: "Printers that need their own driver, USB printers and print queues"
                        chevron: true
                        onClicked: Navigator.open("printersadvanced")
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: page.printer !== null
        spacing: Metrics.sectionGap

        UI.Card {
            Layout.fillWidth: true
            implicitHeight: 96

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 18

                Rectangle {
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    radius: UI.Style.radiusMd
                    color: UI.Style.surfaceTile

                    UI.Icon {
                        anchors.centerIn: parent
                        name: "printer"
                        size: 28
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    UI.Label {
                        Layout.fillWidth: true
                        text: page.printer ? page.printer.info : ""
                        size: 18
                        font.weight: UI.Style.weightSemiBold
                    }

                    UI.Label {
                        Layout.fillWidth: true
                        text: page.printer ? page.statusOf(page.printer) : ""
                        color: page.printer && page.printer.problem ? UI.Style.warning : UI.Style.inkMuted
                    }
                }
            }
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "check"
                    title: "Default printer"
                    description: page.printer && page.printer.isDefault ? "Apps print here unless you choose another printer"
                        : "Make apps print here unless you choose another printer"

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: page.printer !== null && !page.printer.isDefault
                        text: "Set as default"
                        onClicked: Printers.setDefault(page.selected)
                    }

                    UI.Label {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: page.printer !== null && page.printer.isDefault
                        text: "Default"
                        size: 12
                        color: UI.Style.inkMuted
                    }
                }

                UI.SettingRow {
                    iconName: "document"
                    title: "Print a test page"
                    description: "Checks the printer and this computer can talk to each other"

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Print test page"
                        busy: Printers.busy === page.selected
                        enabled: Printers.busy.length === 0 && page.printer !== null && !page.printer.paused
                        onClicked: Printers.printTestPage(page.selected)
                    }
                }

                UI.SettingRow {
                    visible: page.printer !== null && page.printer.jobs > 0
                    iconName: "list"
                    title: "Documents waiting"
                    description: page.printer ? Format.plural(page.printer.jobs, "document is", "documents are") + " waiting to print" : ""

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Cancel all"
                        onClicked: Printers.cancelJobs(page.selected)
                    }
                }

                UI.SettingRow {
                    visible: page.printer !== null && page.printer.paused
                    iconName: "warning"
                    title: "Printing is paused"
                    description: "Documents wait until the printer is resumed"

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Resume"
                        busy: Printers.busy === page.selected
                        enabled: Printers.busy.length === 0
                        onClicked: Printers.resume(page.selected)
                    }
                }

                UI.SettingRow {
                    iconName: "delete"
                    title: "Remove this printer"
                    description: "Apps stop offering it. You can add it again at any time."

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Remove"
                        danger: true
                        enabled: Printers.busy.length === 0
                        onClicked: removeDialog.open()
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            SectionTitle {
                text: "About this printer"
            }

            UI.Card {
                Layout.fillWidth: true

                Column {
                    width: parent.width

                    Repeater {
                        model: page.printer ? [
                            ["Model", page.printer.model],
                            ["Location", page.printer.location],
                            ["Address", page.printer.uri],
                            ["Name apps use", page.printer.name]
                        ].filter(r => r[1] && r[1].length > 0) : []

                        UI.SettingRow {
                            required property var modelData
                            title: modelData[0]

                            UI.Label {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.min(implicitWidth, 420)
                                text: modelData[1]
                                color: UI.Style.inkMuted
                                elide: Text.ElideMiddle
                            }
                        }
                    }
                }
            }
        }
    }

    UI.Dialog {
        id: addressDialog
        title: "Add a printer by its address"
        text: "Type the printer's network name or IP address. You can usually find it on the printer's screen or a page it prints about its network settings."
        onAboutToShow: {
            addressField.text = "";
            addressField.forceActiveFocus();
        }

        UI.TextField {
            id: addressField
            Layout.fillWidth: true
            placeholderText: "For example 192.168.1.20 or printer.local"
            onAccepted: addressAdd.clicked()
        }

        buttons: [
            UI.Button {
                id: addressAdd
                text: "Add"
                kind: "primary"
                enabled: addressField.text.trim().length > 0
                onClicked: {
                    if (!enabled) {
                        return;
                    }
                    addressDialog.close();
                    page.addingUri = "";
                    Printers.addAddress(addressField.text);
                }
            },
            UI.Button {
                text: "Cancel"
                onClicked: addressDialog.close()
            }
        ]
    }

    UI.Dialog {
        id: removeDialog
        title: page.printer ? "Remove " + page.printer.info + "?" : ""
        text: "Documents waiting for it are cancelled. You can add the printer again at any time."
        buttons: [
            UI.Button {
                text: "Remove"
                kind: "primary"
                onClicked: {
                    removeDialog.close();
                    Printers.remove(page.selected);
                }
            },
            UI.Button {
                text: "Cancel"
                onClicked: removeDialog.close()
            }
        ]
    }
}
