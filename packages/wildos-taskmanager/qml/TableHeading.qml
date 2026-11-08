import QtQuick
import WildOS.UI as UI

// Column names over the Startup apps, Windows apps and Services tables, in
// the Processes table's 12px muted style, on a 40px band with the same
// 0.08 hairline under it. A column of width 0 takes the room left over.
Item {
    id: heading

    property var columns: []

    implicitHeight: 40

    Row {
        anchors.fill: parent

        Repeater {
            model: heading.columns

            Item {
                required property var modelData
                required property int index
                width: modelData.width > 0 ? modelData.width : heading.width - heading.fixedWidth()
                height: heading.height

                UI.Label {
                    x: parent.index === 0 ? 16 : 12
                    width: parent.width - x - 12
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: parent.modelData.right ? Text.AlignRight : Text.AlignLeft
                    text: parent.modelData.title
                    size: 12
                    color: UI.Style.inkAlpha(0.7, 0.64)
                }
            }
        }
    }

    function fixedWidth() {
        let w = 0;
        for (const c of columns) {
            w += c.width > 0 ? c.width : 0;
        }
        return w;
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: UI.Style.tone(0.08)
    }
}
