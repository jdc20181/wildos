import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts
import WildOS.Notes
import WildOS.UI as UI

// The left column (AppNotes.dc): All notes and Pinned with their counts,
// the notebooks, the tags, New notebook, and the Quick note card at the
// foot. The board's grey shade under it is the same neutral shade the
// title bar uses on Mica.
Rectangle {
    id: sidebar

    signal newNotebook()
    signal notebookMenu(string name, Item row, real x, real y)

    color: UI.Style.contrast ? "#000000" : Qt.rgba(0, 0, 0, UI.Style.dark ? 0.12 : 0.06)

    function show(kind, key) {
        NoteStore.search = "";
        NoteStore.viewKey = key;
        NoteStore.viewKind = kind;
    }

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: UI.Style.borderSubtle
    }

    Flickable {
        id: flick
        anchors.top: parent.top
        anchors.topMargin: 4
        anchors.bottom: card.top
        anchors.bottomMargin: 8
        width: parent.width - 1
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: UI.ScrollBar {}

        Column {
            id: column
            width: flick.width

            SideRow {
                width: parent.width
                iconName: "all-notes"
                text: qsTr("All notes")
                count: NoteStore.allCount
                current: NoteStore.search.length === 0 && NoteStore.viewKind === "all"
                onClicked: sidebar.show("all", "")
            }
            SideRow {
                width: parent.width
                iconName: "pin"
                text: qsTr("Pinned")
                count: NoteStore.pinnedCount
                current: NoteStore.search.length === 0 && NoteStore.viewKind === "pinned"
                onClicked: sidebar.show("pinned", "")
            }

            SideHeading {
                width: parent.width
                text: qsTr("Notebooks")
            }
            Repeater {
                model: NoteStore.notebooks
                delegate: SideRow {
                    id: notebookRow
                    required property var modelData
                    width: column.width
                    iconName: "notebook"
                    text: modelData.name
                    count: modelData.count
                    current: NoteStore.search.length === 0 && NoteStore.viewKind === "notebook" && NoteStore.viewKey === modelData.name
                    onClicked: sidebar.show("notebook", modelData.name)
                    onMenuRequested: (x, y) => sidebar.notebookMenu(modelData.name, notebookRow, x, y)
                }
            }

            SideHeading {
                visible: NoteStore.tags.length > 0
                width: parent.width
                text: qsTr("Tags")
            }
            Repeater {
                model: NoteStore.tags
                delegate: SideRow {
                    required property var modelData
                    width: column.width
                    iconName: "hash"
                    text: modelData.name
                    current: NoteStore.search.length === 0 && NoteStore.viewKind === "tag" && NoteStore.viewKey === modelData.name
                    onClicked: sidebar.show("tag", modelData.name)
                }
            }

            SideRow {
                width: parent.width
                iconName: "plus"
                text: qsTr("New notebook")
                onClicked: sidebar.newNotebook()
            }
        }
    }

    // The board's second line runs out of the card; here the card grows
    // to hold it.
    Rectangle {
        id: card
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 12
        anchors.rightMargin: 13
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        height: cardRow.implicitHeight + 22
        radius: 10
        color: UI.Style.surfaceRaised
        border.width: 1
        border.color: UI.Style.borderSubtle
        Accessible.role: Accessible.StaticText
        Accessible.name: cardTitle.text + ". " + cardText.text

        RowLayout {
            id: cardRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            NotesIcon {
                name: "keyboard"
                size: 18
                color: UI.Style.inkMuted
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                UI.Label {
                    id: cardTitle
                    Layout.fillWidth: true
                    text: qsTr("Quick note anywhere")
                    size: 12
                    font.weight: UI.Style.weightSemiBold
                }
                UI.Label {
                    id: cardText
                    Layout.fillWidth: true
                    text: qsTr("Super+N, even when Notes is closed")
                    size: 11
                    color: UI.Style.inkMuted
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
