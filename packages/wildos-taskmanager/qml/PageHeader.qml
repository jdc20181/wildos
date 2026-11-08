import QtQuick
import WildOS.UI as UI

// The 54px band over every page: its name in 18px weight 600 at the left,
// the page's buttons at the right, a hairline under it.
Item {
    id: header

    property string title
    default property alias buttons: row.data

    implicitHeight: 55

    UI.Label {
        x: 16
        width: parent.width - x - row.width - 24
        height: 54
        text: header.title
        size: 18
        font.weight: Font.DemiBold
    }

    Row {
        id: row
        anchors.right: parent.right
        anchors.rightMargin: 16
        y: (54 - height) / 2
        spacing: 8
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: UI.Style.divider
    }
}
