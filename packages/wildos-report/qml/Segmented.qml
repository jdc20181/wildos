import QtQuick
import WildOS.UI as UI

// The board's segmented choice ("Just now, Earlier today, Pick a time"): a
// 2px inset strip with a 0.05 fill and 0.10 edge, radius 7; each choice
// 26px, 12px either side, radius 5; the chosen one on a 0.12 fill in
// semibold.
Rectangle {
    id: strip

    property var options: []
    property string value
    signal chosen(string value)

    implicitWidth: row.implicitWidth + 4
    implicitHeight: 32
    radius: 7
    color: Look.field
    border.width: 1
    border.color: Look.fieldEdge

    Row {
        id: row
        x: 2
        y: 2
        spacing: 0

        Repeater {
            model: strip.options

            delegate: Rectangle {
                id: choice
                required property var modelData
                readonly property bool current: strip.value === modelData.value

                width: label.implicitWidth + 24
                height: 26
                radius: 5
                color: current ? Look.selected : mouse.containsMouse ? UI.Style.tone(0.06) : "transparent"
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.text
                Accessible.checked: current

                UI.Label {
                    id: label
                    anchors.centerIn: parent
                    text: choice.modelData.text
                    size: 12.5
                    font.weight: choice.current ? UI.Style.weightSemiBold : UI.Style.weightRegular
                    color: choice.current ? UI.Style.ink : UI.Style.inkMuted
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: strip.chosen(choice.modelData.value)
                }
            }
        }
    }
}
