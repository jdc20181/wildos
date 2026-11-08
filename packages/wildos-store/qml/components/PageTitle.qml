import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A page heading with an optional line under it and actions at the right.
Item {
    id: header

    property string title
    property string subtitle
    default property alias actions: actionRow.data

    Layout.fillWidth: true
    implicitHeight: Math.max(textColumn.implicitHeight, actionRow.implicitHeight)

    Column {
        id: textColumn
        width: parent.width - actionRow.implicitWidth - 24
        spacing: 6

        UI.Label {
            width: parent.width
            text: header.title
            size: 28
            font.weight: Font.DemiBold
            font.letterSpacing: -0.28
            Accessible.role: Accessible.Heading
        }

        UI.Label {
            visible: header.subtitle.length > 0
            width: parent.width
            text: header.subtitle
            size: 14
            color: Metrics.inkSub
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }
    }

    Row {
        id: actionRow
        anchors.right: parent.right
        anchors.verticalCenter: textColumn.verticalCenter
        spacing: 10
    }
}
