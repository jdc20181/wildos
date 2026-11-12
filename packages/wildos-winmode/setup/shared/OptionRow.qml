import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// One row of 9.4's lists: an optional 20px control at the left (a tick box,
// or room for one, so a row that belongs to the one above lines up under
// its text), a 14px title over a 12px muted line, and whatever sits at the
// right. 48px at least, 7 / 18 padding, 14 between a tick box and its text
// and 16 otherwise, a hairline between rows. A row with a tick box ticks it
// wherever it is clicked.
Item {
    id: row

    property string title
    property string description
    property color descriptionColor: UI.Style.inkMuted
    property bool divider: y > 0
    property alias leading: leadingHolder.data
    default property alias trailing: trailingRow.data
    // Clicking anywhere on the row does this (the tick box rows).
    property bool interactive: false
    signal clicked()

    Layout.fillWidth: true
    implicitHeight: Math.max(48, Math.max(text.implicitHeight, trailingRow.implicitHeight) + 14)
    Accessible.role: Accessible.ListItem
    Accessible.name: title
    Accessible.description: description

    Rectangle {
        visible: row.divider
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: UI.Style.divider
    }

    MouseArea {
        anchors.fill: parent
        enabled: row.interactive
        cursorShape: row.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: row.clicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: leadingHolder.visible ? 14 : 16

        Item {
            id: leadingHolder
            visible: children.length > 0
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
        }

        ColumnLayout {
            id: text
            Layout.fillWidth: true
            spacing: 2

            UI.Label {
                Layout.fillWidth: true
                text: row.title
                size: 14
            }

            UI.Label {
                Layout.fillWidth: true
                visible: row.description.length > 0
                text: row.description
                size: 12
                color: row.descriptionColor
                wrapMode: Text.Wrap
                elide: Text.ElideNone
            }
        }

        RowLayout {
            id: trailingRow
            visible: children.length > 0
            spacing: 10
        }
    }
}
