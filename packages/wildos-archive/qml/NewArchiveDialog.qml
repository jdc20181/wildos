import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Dialogs
import QtQuick.Layouts
import WildOS.Archive
import WildOS.UI as UI

// New archive: the files and folders to put in, the name, ZIP or 7z, and
// where it goes. From Add files it starts with the open archive's
// contents, which go into the new archive; the open one is left as it is.
UI.Dialog {
    id: dialog

    signal created(string path, string message)
    signal failed(string title, string message, var details)

    function startWith(urls) {
        maker.reset(urls);
        open();
        nameField.forceActiveFocus();
        nameField.selectAll();
    }
    function startFrom(archive, urls) {
        maker.resetFrom(archive, urls);
        open();
        nameField.forceActiveFocus();
        nameField.selectAll();
    }

    size: "large"
    title: maker.sourceName.length > 0 ? qsTr("Add files to a new archive") : qsTr("New archive")
    text: maker.sourceName.length > 0
        ? qsTr("%1 stays as it is. The new archive holds everything in it, plus the files you add here.").arg(maker.sourceName)
        : qsTr("Pick the files and folders to put in it. ZIP opens anywhere; 7z packs smaller.")
    closePolicy: maker.busy ? C.Popup.NoAutoClose : C.Popup.CloseOnEscape

    ArchiveMaker {
        id: maker
        onDone: (ok, path, message, details) => {
            if (ok) {
                dialog.close();
                dialog.created(path, message);
            } else if (message.length > 0) {
                dialog.failed(qsTr("Couldn't make the archive"), message, details);
            }
        }
    }

    FileDialog {
        id: addFilesDialog
        title: qsTr("Add files")
        fileMode: FileDialog.OpenFiles
        onAccepted: maker.addUrls(selectedFiles)
    }
    FolderDialog {
        id: addFolderDialog
        title: qsTr("Add a folder")
        onAccepted: maker.addUrls([selectedFolder])
    }
    FolderDialog {
        id: saveInDialog
        title: qsTr("Save the archive in")
        onAccepted: maker.setFolderUrl(selectedFolder)
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 12
        rowSpacing: 10
        enabled: !maker.busy

        UI.Label {
            text: qsTr("Name")
            size: 13
            color: UI.Style.inkMuted
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            UI.TextField {
                id: nameField
                Layout.fillWidth: true
                text: maker.name
                error: maker.problem.length > 0 && text.length > 0 && maker.problem.indexOf("name") >= 0
                onTextEdited: maker.name = text
                onAccepted: maker.create()
            }
            UI.Label {
                text: maker.format === "7z" ? ".7z" : ".zip"
                size: 13
                color: UI.Style.inkMuted
            }
            Row {
                spacing: 4
                UI.SegmentButton {
                    text: "ZIP"
                    checked: maker.format === "zip"
                    onClicked: maker.format = "zip"
                }
                UI.SegmentButton {
                    text: "7z"
                    checked: maker.format === "7z"
                    onClicked: maker.format = "7z"
                }
            }
        }

        UI.Label {
            text: qsTr("Save in")
            size: 13
            color: UI.Style.inkMuted
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                radius: 6
                color: ArchiveStyle.fieldFill
                border.width: 1
                border.color: ArchiveStyle.fieldEdge

                UI.Label {
                    anchors.fill: parent
                    anchors.leftMargin: 11
                    anchors.rightMargin: 10
                    verticalAlignment: Text.AlignVCenter
                    text: maker.folderText
                    size: 13
                    elide: Text.ElideLeft
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        saveInDialog.currentFolder = maker.folderUrl();
                        saveInDialog.open();
                    }
                }
            }
            ToolButton {
                side: 34
                iconName: "folder-open"
                tip: qsTr("Choose folder")
                onClicked: {
                    saveInDialog.currentFolder = maker.folderUrl();
                    saveInDialog.open();
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: 6
        Layout.preferredHeight: Math.min(5, Math.max(1, list.count + (maker.sourceName.length > 0 ? 1 : 0))) * 40 + 8
        radius: 8
        color: ArchiveStyle.fieldFill
        border.width: 1
        border.color: ArchiveStyle.fieldEdge

        UI.Label {
            anchors.centerIn: parent
            visible: list.count === 0 && maker.sourceName.length === 0
            text: qsTr("Nothing added yet")
            size: 13
            color: UI.Style.inkSubtle
        }

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: 4
            clip: true
            model: maker.items
            boundsBehavior: Flickable.StopAtBounds
            C.ScrollBar.vertical: UI.ScrollBar {}

            header: Item {
                width: ListView.view.width
                height: maker.sourceName.length > 0 ? 40 : 0
                visible: height > 0

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10
                    Image {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        source: ArchiveStyle.icon
                        sourceSize.width: 36
                        sourceSize.height: 36
                    }
                    UI.Label {
                        Layout.fillWidth: true
                        text: qsTr("Everything in %1").arg(maker.sourceName)
                        size: 13
                        elide: Text.ElideMiddle
                    }
                }
            }

            delegate: Item {
                id: itemRow
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 40

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 4
                    spacing: 10

                    ArchiveIcon {
                        name: itemRow.modelData.isDir ? "folder" : "file-text"
                        size: 18
                        strokeWidth: 1.8
                        color: itemRow.modelData.isDir ? ArchiveStyle.folderInk : ArchiveStyle.fileInk
                    }
                    UI.Label {
                        Layout.fillWidth: true
                        text: itemRow.modelData.name
                        size: 13
                        elide: Text.ElideMiddle
                    }
                    UI.Label {
                        Layout.maximumWidth: 200
                        text: itemRow.modelData.folder
                        size: 12
                        color: UI.Style.inkSubtle
                        elide: Text.ElideLeft
                    }
                    UI.Label {
                        Layout.preferredWidth: 70
                        horizontalAlignment: Text.AlignRight
                        text: itemRow.modelData.sizeText
                        size: 12.5
                        color: UI.Style.inkMuted
                    }
                    ToolButton {
                        side: 28
                        iconSize: 14
                        iconName: "x"
                        tip: qsTr("Leave out")
                        enabled: !maker.busy
                        onClicked: maker.removeAt(itemRow.index)
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 4
        enabled: !maker.busy

        ToolButton {
            iconName: "add-files"
            text: qsTr("Add files")
            onClicked: addFilesDialog.open()
        }
        ToolButton {
            iconName: "folder-plus"
            text: qsTr("Add a folder")
            onClicked: addFolderDialog.open()
        }
        Item {
            Layout.fillWidth: true
        }
        UI.Label {
            text: maker.totalText
            size: 12.5
            color: UI.Style.inkMuted
        }
    }

    UI.Label {
        Layout.fillWidth: true
        visible: !maker.busy && maker.problem.length > 0 && (maker.items.length > 0 || maker.sourceName.length > 0 || nameField.text.length === 0)
        text: maker.problem
        size: 12.5
        color: UI.Style.danger
        wrapMode: Text.Wrap
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: maker.busy
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            UI.Label {
                Layout.fillWidth: true
                text: maker.progressText
                size: 12.5
                elide: Text.ElideMiddle
            }
            UI.Label {
                text: Math.round(maker.progress * 100) + "%"
                size: 12.5
                color: UI.Style.inkMuted
            }
        }
        UI.ProgressBar {
            Layout.fillWidth: true
            value: maker.progress
        }
    }

    buttons: [
        UI.Button {
            text: qsTr("Cancel")
            onClicked: {
                if (maker.busy) {
                    maker.cancel();
                } else {
                    dialog.close();
                }
            }
        },
        UI.Button {
            kind: "primary"
            text: qsTr("Make archive")
            enabled: !maker.busy && maker.problem.length === 0
            onClicked: maker.create()
        }
    ]
}
