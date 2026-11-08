pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WildOS.UI as UI

// The details list of DialogSave.dc: a 30px header (Name, Date modified
// 150, Type 130, Size 70) that sorts when clicked, then 36px rows with a
// 20px icon, the name in 13px and the rest in 12px muted. Click selects
// (Ctrl and Shift add when several may be opened), double click opens a
// folder or chooses the file, and typing jumps to a name.
FocusScope {
    id: list

    required property var folder
    property bool showSize: true
    property int renameRow: -1
    property string typed: ""

    signal activated(int row)
    signal emptyAction(string action)
    property string emptyText: ""
    property string emptyButton: ""
    property string emptyActionName: ""

    function startRename(row) {
        view.positionViewAtIndex(row, ListView.Contain);
        renameRow = row;
    }

    function pageRows() {
        return Math.max(1, Math.floor(view.height / 36) - 1);
    }

    Keys.onPressed: event => {
        const f = list.folder;
        const mods = event.modifiers & (Qt.ControlModifier | Qt.ShiftModifier);
        const cur = f.currentIndex;
        if (event.key === Qt.Key_Down) {
            f.moveCurrent(cur + 1, mods);
        } else if (event.key === Qt.Key_Up) {
            f.moveCurrent(Math.max(0, cur - 1), mods);
        } else if (event.key === Qt.Key_Home) {
            f.moveCurrent(0, mods);
        } else if (event.key === Qt.Key_End) {
            f.moveCurrent(f.count - 1, mods);
        } else if (event.key === Qt.Key_PageDown) {
            f.moveCurrent(cur + pageRows(), mods);
        } else if (event.key === Qt.Key_PageUp) {
            f.moveCurrent(Math.max(0, cur - pageRows()), mods);
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (cur >= 0) {
                list.activated(cur);
            }
        } else if (event.key === Qt.Key_Space && (event.modifiers & Qt.ControlModifier) && cur >= 0) {
            f.select(cur, Qt.ControlModifier);
        } else if (event.key === Qt.Key_F2 && cur >= 0 && f.writable) {
            list.startRename(cur);
        } else if (event.text.length === 1 && event.text >= " " && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier))) {
            typed += event.text;
            typeReset.restart();
            const row = f.findPrefix(typed, typed.length > 1 ? Math.max(0, cur) : cur + 1);
            if (row >= 0) {
                f.select(row, 0);
            }
        } else {
            return;
        }
        event.accepted = true;
    }

    Timer {
        id: typeReset
        interval: 900
        onTriggered: list.typed = ""
    }

    Connections {
        target: list.folder
        function onCurrentIndexChanged() {
            if (list.folder.currentIndex >= 0) {
                view.positionViewAtIndex(list.folder.currentIndex, ListView.Contain);
            }
        }
        function onUrlChanged() {
            list.renameRow = -1;
            view.positionViewAtBeginning();
        }
        function onFolderMade(row) {
            list.startRename(row);
        }
    }

    Item {
        id: header
        x: 0
        width: parent.width
        height: 30

        component HeaderCell: Item {
            id: cell
            required property int column
            required property string label
            property bool alignRight: false
            readonly property bool sorted: list.folder.sortColumn === column
            height: header.height
            Accessible.role: Accessible.ColumnHeader
            Accessible.name: label

            Row {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: cell.alignRight ? parent.right : undefined
                spacing: 4
                layoutDirection: cell.alignRight ? Qt.RightToLeft : Qt.LeftToRight

                UI.Label {
                    text: cell.label
                    size: 12
                    color: UI.Style.inkMuted
                    font.weight: cell.sorted ? Font.Medium : Font.Normal
                }

                UI.Icon {
                    visible: cell.sorted
                    anchors.verticalCenter: parent.verticalCenter
                    name: list.folder.sortDescending ? "chevron-down" : "chevron-up"
                    size: 10
                    color: UI.Style.inkMuted
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (cell.sorted) {
                        list.folder.sortDescending = !list.folder.sortDescending;
                    } else {
                        list.folder.sortColumn = cell.column;
                    }
                }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 10

            Item { Layout.preferredWidth: 20 }
            HeaderCell { Layout.fillWidth: true; column: 0; label: "Name" }
            HeaderCell { Layout.preferredWidth: 150; column: 1; label: "Date modified"; visible: !list.folder.isComputer }
            HeaderCell { Layout.preferredWidth: 130; column: 2; label: "Type" }
            HeaderCell { Layout.preferredWidth: 70; column: 3; label: list.folder.isComputer ? "Free" : "Size"; alignRight: true }
        }
    }

    ListView {
        id: view
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        model: list.folder
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        activeFocusOnTab: false
        ScrollBar.vertical: UI.ScrollBar {}

        delegate: Item {
            id: row

            required property int index
            required property string name
            required property string fileName
            required property string iconName
            required property bool isDir
            required property string date
            required property string type
            required property string size
            required property bool selected
            required property bool hidden

            width: view.width
            height: 36
            Accessible.role: Accessible.Row
            Accessible.name: name
            Accessible.selected: selected

            Rectangle {
                anchors.fill: parent
                radius: UI.Style.radiusXs
                color: row.selected ? UI.Style.accentSelected : rowMouse.containsMouse ? UI.Style.surfaceHover : "transparent"
                border.width: list.activeFocus && list.folder.currentIndex === row.index ? 1 : 0
                border.color: UI.Style.accentBorderStrong
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                UI.ThemeIcon {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    name: row.iconName
                    fallback: row.isDir ? "folder" : "text-x-generic"
                    opacity: row.hidden ? 0.5 : 1
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    UI.Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        visible: list.renameRow !== row.index
                        text: row.name
                        opacity: row.hidden ? 0.6 : 1
                    }

                    Loader {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        active: list.renameRow === row.index
                        sourceComponent: UI.TextField {
                            id: renameField
                            height: 28
                            implicitHeight: 28
                            text: row.name
                            escapeClears: false
                            Component.onCompleted: {
                                forceActiveFocus();
                                selectAll();
                            }
                            property bool finished: false
                            function commit() {
                                if (finished) {
                                    return;
                                }
                                finished = true;
                                list.folder.renameItem(row.index, text);
                                list.renameRow = -1;
                                list.forceActiveFocus();
                            }
                            onAccepted: commit()
                            onActiveFocusChanged: if (!activeFocus) commit()
                            Keys.onEscapePressed: {
                                finished = true;
                                list.renameRow = -1;
                                list.forceActiveFocus();
                            }
                        }
                    }
                }

                UI.Label {
                    Layout.preferredWidth: 150
                    visible: !list.folder.isComputer
                    text: row.date
                    size: 12
                    color: UI.Style.inkMuted
                }

                UI.Label {
                    Layout.preferredWidth: 130
                    text: row.type
                    size: 12
                    color: UI.Style.inkMuted
                }

                UI.Label {
                    Layout.preferredWidth: 70
                    horizontalAlignment: Text.AlignRight
                    text: row.size
                    size: 12
                    color: UI.Style.inkMuted
                }
            }

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                enabled: list.renameRow !== row.index
                onPressed: mouse => {
                    list.forceActiveFocus();
                    list.folder.select(row.index, mouse.modifiers);
                }
                onDoubleClicked: list.activated(row.index)
            }
        }

        // Clicking below the last row clears the selection, as in Files.
        MouseArea {
            z: -1
            width: view.width
            height: Math.max(0, view.height - view.contentHeight)
            y: view.contentHeight
            onPressed: {
                list.forceActiveFocus();
                list.folder.clearSelection();
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: view
        width: Math.min(view.width - 40, 360)
        visible: list.emptyText.length > 0 && view.count === 0
        spacing: 12

        UI.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: list.emptyText
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }

        UI.Button {
            Layout.alignment: Qt.AlignHCenter
            visible: list.emptyButton.length > 0
            text: list.emptyButton
            onClicked: list.emptyAction(list.emptyActionName)
        }
    }
}
