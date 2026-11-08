import QtQuick
import WildOS.UI as UI

// One row in a RowGroup: a 14px title, an optional 12px line under it, and
// whatever controls are given on the right.
Item {
    id: row

    property string title
    property string subtitle
    property int minHeight: 62
    property bool warning: false
    property bool last: false
    default property alias trailing: trailingRow.data

    width: parent ? parent.width : implicitWidth
    implicitHeight: visible ? Math.max(minHeight, texts.implicitHeight + 20) : 0
    height: implicitHeight

    Rectangle {
        anchors.fill: parent
        visible: row.warning
        color: UI.Style.warningSoft
    }

    UI.Icon {
        id: warnIcon
        visible: row.warning
        x: 18
        anchors.verticalCenter: parent.verticalCenter
        name: "warning"
        size: 18
        color: UI.Style.warning
    }

    Column {
        id: texts
        x: row.warning ? 18 + 18 + 14 : 18
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - x - 18 - (trailingRow.implicitWidth > 0 ? trailingRow.implicitWidth + 16 : 0)
        spacing: 2

        UI.Label {
            width: parent.width
            text: row.title
            size: 14
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }

        UI.Label {
            visible: row.subtitle.length > 0
            width: parent.width
            text: row.subtitle
            size: 12
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }
    }

    Row {
        id: trailingRow
        anchors.right: parent.right
        anchors.rightMargin: 18
        height: parent.height
        spacing: 16
    }

    Rectangle {
        visible: !row.last
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: UI.Style.divider
    }
}
