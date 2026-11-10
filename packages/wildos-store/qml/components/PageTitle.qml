import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A page's 32px bold title, an optional line under it, and anything given
// (a button, filter pills) at the right, level with the bottom of the text.
Item {
    id: header

    property string title
    property string subtitle
    default property alias actions: actionRow.data
    // Pills too wide to sit beside the title go under it instead.
    readonly property bool stacked: actionRow.implicitWidth > 0 && textColumn.titleWidth + actionRow.implicitWidth + 32 > width

    Layout.fillWidth: true
    implicitHeight: stacked ? textColumn.implicitHeight + 16 + actionRow.implicitHeight : Math.max(textColumn.implicitHeight, actionRow.implicitHeight)

    Column {
        id: textColumn
        readonly property real titleWidth: Math.max(titleLabel.implicitWidth, subtitleLabel.visible ? subtitleLabel.implicitWidth : 0)
        width: header.stacked || actionRow.implicitWidth === 0 ? parent.width : Math.min(titleWidth, parent.width - actionRow.implicitWidth - 32)
        spacing: 4

        UI.Label {
            id: titleLabel
            width: parent.width
            text: header.title
            size: 32
            font.weight: Font.Bold
            font.letterSpacing: -0.64
            Accessible.role: Accessible.Heading
        }

        UI.Label {
            id: subtitleLabel
            visible: header.subtitle.length > 0
            width: parent.width
            text: header.subtitle
            size: 15
            color: Metrics.inkSecondary
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }
    }

    Row {
        id: actionRow
        x: header.stacked ? 0 : header.width - width
        y: header.stacked ? textColumn.implicitHeight + 16 : textColumn.implicitHeight - height
        spacing: 8
    }
}
