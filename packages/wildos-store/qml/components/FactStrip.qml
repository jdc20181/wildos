import QtQuick
import WildOS.UI as UI

// The app page's strip of facts under the title (StoreAppPage.dc.html):
// hairlines above and below, cells split evenly with a hairline between,
// each a small capitalised label, the value and a quiet line under it.
Item {
    id: facts
    implicitHeight: factRow.implicitHeight + 28

    // Each {label, value, sub}, or {label, icon, sub} for a picture
    // in place of the value.
    property var cells: []

    Rectangle {
        width: parent.width
        height: 1
        color: Metrics.divider
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Metrics.divider
    }

    Row {
        id: factRow
        y: 14
        width: parent.width

        Repeater {
            model: facts.cells

            Item {
                id: cell
                required property var modelData
                required property int index
                width: factRow.width / facts.cells.length
                implicitHeight: cellColumn.implicitHeight

                Rectangle {
                    visible: cell.index > 0
                    width: 1
                    height: parent.height
                    color: Metrics.divider
                }

                Column {
                    id: cellColumn
                    x: 8
                    width: parent.width - 16
                    spacing: 5

                    UI.Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: cell.modelData.label.toUpperCase()
                        size: 10.5
                        font.weight: Font.Bold
                        font.letterSpacing: 0.74
                        color: Metrics.inkTertiary
                    }

                    Item {
                        width: parent.width
                        height: 24

                        UI.Label {
                            visible: !cell.modelData.icon
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: cell.modelData.value
                            size: 18
                            font.weight: Font.Bold
                            color: Metrics.inkSecondary
                        }

                        UI.Icon {
                            visible: !!cell.modelData.icon
                            anchors.centerIn: parent
                            name: cell.modelData.icon || ""
                            size: 22
                            strokeWidth: 2
                            color: Metrics.success
                        }
                    }

                    UI.Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        // An empty line keeps every cell the same height.
                        text: cell.modelData.sub || " "
                        size: 12
                        color: Metrics.inkTertiary
                    }
                }
            }
        }
    }
}
