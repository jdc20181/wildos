pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// Copying files to and from a server (RemoteFiles.dc): this PC on the left,
// the server on the right, the arrows between them, and the copies in
// progress below.
Item {
    id: view

    property var tab: null
    readonly property var files: tab ? tab.files : null

    signal notice(string text)

    Connections {
        target: view.files
        function onNotice(text) {
            view.notice(text);
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#0b0f1c"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        anchors.topMargin: 18
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            FilePane {
                id: localPane
                Layout.fillWidth: true
                Layout.fillHeight: true
                remote: false
                title: qsTr("This PC")
                path: view.files ? view.files.localShown : ""
                entries: view.files ? view.files.localEntries : []
                onOpenFolder: name => view.files.openLocal(name)
                onOpenFile: name => view.files.openLocalFile(name)
                onBack: view.files.localBack()
                onRefresh: view.files.refreshLocal()
                onNewFolder: newFolderDialog.ask(false)
            }

            Column {
                Layout.alignment: Qt.AlignVCenter
                spacing: 12
                Arrow {
                    iconName: "chevron-right"
                    primary: localPane.selection.length > 0
                    enabled: localPane.selection.length > 0 && view.files && view.files.remoteReady
                    tip: qsTr("Upload to %1").arg(view.tab ? view.tab.title : "")
                    onClicked: {
                        view.files.upload(localPane.selection);
                        localPane.selection = [];
                    }
                }
                Arrow {
                    iconName: "chevron-left"
                    primary: remotePane.selection.length > 0
                    enabled: remotePane.selection.length > 0
                    tip: qsTr("Download to this PC")
                    onClicked: {
                        view.files.download(remotePane.selection);
                        remotePane.selection = [];
                    }
                }
            }

            FilePane {
                id: remotePane
                Layout.fillWidth: true
                Layout.fillHeight: true
                remote: true
                title: view.tab ? view.tab.title : ""
                tint: view.tab ? view.tab.colour : "#5fd39a"
                path: view.files ? view.files.remotePath : ""
                entries: view.files ? view.files.remoteEntries : []
                loading: view.files ? view.files.loading : false
                error: view.files ? view.files.remoteError : ""
                onOpenFolder: name => view.files.openRemote(name)
                onOpenFile: name => view.files.editRemote(name)
                onBack: view.files.remoteBack()
                onRefresh: view.files.remoteReady ? view.files.refreshRemote() : view.files.retry()
                onNewFolder: newFolderDialog.ask(true)
                onDeleteRequested: names => deleteDialog.ask(names)
                onRenameRequested: name => renameDialog.ask(name)
            }
        }

        // Copies in progress.
        Rectangle {
            visible: view.files !== null && view.files.transfers.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(transferColumn.implicitHeight + 28, 220)
            radius: 12
            color: UI.Style.tone(0.035)
            border.width: 1
            border.color: UI.Style.tone(0.08)
            Column {
                id: transferColumn
                x: 16
                y: 14
                width: parent.width - 32
                spacing: 12
                Item {
                    width: parent.width
                    height: 20
                    UI.Label {
                        text: view.files ? view.files.transferTitle : ""
                        size: 14
                        font.weight: Font.Bold
                    }
                    Row {
                        anchors.right: parent.right
                        spacing: 14
                        UI.Label {
                            text: view.files ? view.files.transferSummary : ""
                            size: 13
                            color: UI.Style.inkMuted
                        }
                        UI.LinkButton {
                            visible: view.files !== null && !view.files.transferring
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Clear")
                            onClicked: view.files.clearFinished()
                        }
                        UI.LinkButton {
                            visible: view.files !== null && view.files.transferring
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Stop")
                            onClicked: view.files.cancelTransfers()
                        }
                    }
                }
                Repeater {
                    model: view.files ? view.files.transfers.slice(-6) : []
                    Item {
                        id: transfer
                        required property var modelData
                        width: transferColumn.width
                        height: 26
                        UI.Glyph {
                            x: 2
                            anchors.verticalCenter: parent.verticalCenter
                            width: 14
                            height: 14
                            path: transfer.modelData.upload ? RemoteIcons.upload : RemoteIcons.download
                            color: transfer.modelData.state === "failed" ? UI.Style.danger : "#5fd39a"
                        }
                        UI.Label {
                            x: 28
                            anchors.verticalCenter: parent.verticalCenter
                            width: 180
                            text: transfer.modelData.name
                            size: 13
                        }
                        Rectangle {
                            x: 222
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 222 - 150
                            height: 4
                            radius: 2
                            color: UI.Style.tone(0.1)
                            Rectangle {
                                width: parent.width * Math.max(0, Math.min(1, transfer.modelData.progress))
                                height: parent.height
                                radius: 2
                                color: transfer.modelData.state === "done" ? "#5fd39a" : transfer.modelData.state === "failed" ? UI.Style.danger : "#8fb4ff"
                            }
                        }
                        UI.Label {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 136
                            horizontalAlignment: Text.AlignRight
                            text: transfer.modelData.detail
                            size: 13
                            color: transfer.modelData.state === "failed" ? UI.Style.danger : UI.Style.inkMuted
                        }
                    }
                }
            }
        }

        Row {
            spacing: 10
            UI.Glyph {
                anchors.verticalCenter: parent.verticalCenter
                width: 14
                height: 14
                path: RemoteIcons.file
                color: UI.Style.iconMuted
            }
            UI.Label {
                text: qsTr("Double-click a file on the server to open it here. Saving it uploads it back.")
                size: 13
                color: UI.Style.inkMuted
            }
        }
    }

    UI.Dialog {
        id: newFolderDialog
        property bool remote
        function ask(r) {
            remote = r;
            title = qsTr("New folder");
            folderName.text = "";
            open();
            folderName.forceActiveFocus();
        }
        UI.TextField {
            id: folderName
            Layout.fillWidth: true
            placeholderText: qsTr("Name")
            onAccepted: folderMake.clicked()
        }
        buttons: [
            UI.Button {
                text: qsTr("Cancel")
                onClicked: newFolderDialog.close()
            },
            UI.Button {
                id: folderMake
                kind: "primary"
                text: qsTr("Make")
                enabled: folderName.text.trim().length > 0
                onClicked: {
                    newFolderDialog.remote ? view.files.newRemoteFolder(folderName.text) : view.files.newLocalFolder(folderName.text);
                    newFolderDialog.close();
                }
            }
        ]
    }
    UI.Dialog {
        id: deleteDialog
        property var names: []
        function ask(n) {
            names = n;
            title = n.length === 1 ? qsTr("Delete %1 from the server?").arg(n[0]) : qsTr("Delete %1 items from the server?").arg(n.length);
            text = qsTr("There's no bin on the server: this can't be undone.");
            open();
        }
        buttons: [
            UI.Button {
                text: qsTr("Cancel")
                onClicked: deleteDialog.close()
            },
            UI.Button {
                kind: "primary"
                danger: true
                text: qsTr("Delete")
                onClicked: {
                    view.files.removeRemote(deleteDialog.names);
                    remotePane.selection = [];
                    deleteDialog.close();
                }
            }
        ]
    }
    UI.Dialog {
        id: renameDialog
        property string from
        function ask(name) {
            from = name;
            title = qsTr("Rename %1").arg(name);
            renameField.text = name;
            open();
            renameField.forceActiveFocus();
            renameField.selectAll();
        }
        UI.TextField {
            id: renameField
            Layout.fillWidth: true
            onAccepted: renameGo.clicked()
        }
        buttons: [
            UI.Button {
                text: qsTr("Cancel")
                onClicked: renameDialog.close()
            },
            UI.Button {
                id: renameGo
                kind: "primary"
                text: qsTr("Rename")
                enabled: renameField.text.trim().length > 0 && renameField.text !== renameDialog.from
                onClicked: {
                    view.files.renameRemote(renameDialog.from, renameField.text);
                    renameDialog.close();
                }
            }
        ]
    }

    component Arrow: UI.IconButton {
        property bool primary: false
        width: 40
        height: 40
        radius: 20
        iconSize: 16
        selected: primary && enabled
        iconColor: UI.Style.ink
    }

    component FilePane: Rectangle {
        id: pane
        property bool remote
        property string title
        property string path
        property var entries: []
        property bool loading: false
        property string error
        property color tint: "#8fb4ff"
        property var selection: []
        property int anchorIndex: -1
        signal openFolder(string name)
        signal openFile(string name)
        signal back()
        signal refresh()
        signal newFolder()
        signal deleteRequested(var names)
        signal renameRequested(string name)

        onPathChanged: {
            selection = [];
            anchorIndex = -1;
        }

        radius: 12
        color: UI.Style.tone(0.035)
        border.width: 1
        border.color: UI.Style.tone(0.08)

        function choose(index, name, mods) {
            if (mods & Qt.ShiftModifier && anchorIndex >= 0) {
                const a = Math.min(anchorIndex, index);
                const b = Math.max(anchorIndex, index);
                selection = shownEntries.slice(a, b + 1).map(e => e.name);
            } else if (mods & Qt.ControlModifier) {
                const s = selection.slice();
                const i = s.indexOf(name);
                i >= 0 ? s.splice(i, 1) : s.push(name);
                selection = s;
                anchorIndex = index;
            } else {
                selection = [name];
                anchorIndex = index;
            }
        }
        // This PC's dot files stay out of the way; the server's are often
        // the point (.htaccess, .env).
        readonly property var shownEntries: remote ? entries : entries.filter(e => !e.hidden)

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                Rectangle {
                    x: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: 26
                    height: 26
                    radius: 7
                    color: Qt.rgba(pane.tint.r, pane.tint.g, pane.tint.b, 0.16)
                    UI.Icon {
                        anchors.centerIn: parent
                        name: pane.remote ? "terminal" : "monitor"
                        size: 14
                        color: pane.tint
                    }
                }
                Row {
                    x: 52
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10
                    UI.Label {
                        text: pane.title
                        size: 14
                        font.weight: Font.Bold
                    }
                    UI.Label {
                        text: pane.path
                        size: 13
                        font.family: "JetBrains Mono"
                        color: UI.Style.inkMuted
                    }
                }
                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    UI.IconButton { width: 32; height: 32; iconName: "chevron-left"; iconSize: 14; iconColor: UI.Style.iconMuted; tip: qsTr("Back"); onClicked: pane.back() }
                    UI.IconButton { width: 32; height: 32; iconName: "refresh"; iconSize: 14; iconColor: UI.Style.iconMuted; tip: qsTr("Refresh"); onClicked: pane.refresh() }
                    UI.IconButton { width: 32; height: 32; iconName: "plus"; iconSize: 14; iconColor: UI.Style.iconMuted; tip: qsTr("New folder"); onClicked: pane.newFolder() }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: UI.Style.tone(0.07)
                }
            }
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                UI.Label { x: 14; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Name"); size: 12; color: UI.Style.inkSubtle }
                UI.Label { x: parent.width - 260; width: 110; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Size"); size: 12; color: UI.Style.inkSubtle }
                UI.Label { anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Modified"); size: 12; color: UI.Style.inkSubtle }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: UI.Style.tone(0.07)
                }
            }
            ListView {
                id: fileList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: pane.shownEntries
                boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    id: fileRow
                    required property var modelData
                    required property int index
                    readonly property bool chosen: pane.selection.indexOf(modelData.name) >= 0
                    width: ListView.view.width
                    height: 38
                    color: chosen ? UI.Style.tone(0.09) : rowMouse.containsMouse ? UI.Style.tone(0.04) : "transparent"
                    UI.Glyph {
                        x: 16
                        anchors.verticalCenter: parent.verticalCenter
                        width: 15
                        height: 15
                        path: fileRow.modelData.dir ? RemoteIcons.folder : RemoteIcons.file
                        color: fileRow.modelData.dir ? "#ffc46a" : UI.Style.iconMuted
                    }
                    UI.Label {
                        x: 40
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 40 - 270
                        text: fileRow.modelData.name
                        size: 14
                    }
                    UI.Label {
                        x: parent.width - 260
                        width: 110
                        horizontalAlignment: Text.AlignRight
                        anchors.verticalCenter: parent.verticalCenter
                        text: fileRow.modelData.sizeText
                        size: 13
                        color: UI.Style.inkMuted
                    }
                    UI.Label {
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: fileRow.modelData.modified
                        size: 13
                        color: UI.Style.inkMuted
                    }
                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: m => {
                            if (m.button === Qt.RightButton) {
                                if (!fileRow.chosen) {
                                    pane.choose(fileRow.index, fileRow.modelData.name, 0);
                                }
                                if (pane.remote) {
                                    rowMenu.entries = [
                                        { text: fileRow.modelData.dir ? qsTr("Open") : qsTr("Open here"), icon: "open", bold: true,
                                          run: () => fileRow.modelData.dir ? pane.openFolder(fileRow.modelData.name) : pane.openFile(fileRow.modelData.name) },
                                        { text: qsTr("Download"), icon: "download", run: () => { view.files.download(pane.selection); pane.selection = []; } },
                                        { text: qsTr("Rename"), icon: "rename", enabled: pane.selection.length === 1, run: () => pane.renameRequested(fileRow.modelData.name) },
                                        { separator: true },
                                        { text: qsTr("Delete"), icon: "delete", danger: true, run: () => pane.deleteRequested(pane.selection) }
                                    ];
                                    rowMenu.popup(fileRow, m.x, m.y);
                                }
                                return;
                            }
                            pane.choose(fileRow.index, fileRow.modelData.name, m.modifiers);
                        }
                        onDoubleClicked: fileRow.modelData.dir ? pane.openFolder(fileRow.modelData.name) : pane.openFile(fileRow.modelData.name)
                    }
                }
            }
        }

        Column {
            visible: pane.remote && (pane.error.length > 0 || (pane.loading && pane.entries.length === 0))
            anchors.centerIn: parent
            width: parent.width - 60
            spacing: 14
            UI.Spinner {
                visible: pane.error.length === 0
                anchors.horizontalCenter: parent.horizontalCenter
                size: 22
            }
            UI.Label {
                visible: pane.error.length > 0
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: pane.error
                size: 14
                color: UI.Style.inkMuted
                wrapMode: Text.Wrap
            }
            UI.Button {
                visible: pane.error.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Try again")
                onClicked: view.files.retry()
            }
        }

        UI.Menu {
            id: rowMenu
        }
    }
}
