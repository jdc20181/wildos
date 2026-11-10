import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// One of the cards under "What it can reach": a tinted badge and a title,
// then a line per thing with its icon, or a sentence instead.
Rectangle {
    id: card

    property string title
    property string iconName
    property color tint: UI.Style.accent
    // Each {icon, label} from StoreApp, or {tick: true, label}.
    property var rows: []
    property string text

    Layout.fillWidth: true
    Layout.preferredWidth: 1
    Layout.alignment: Qt.AlignTop
    implicitHeight: column.implicitHeight + 36
    radius: Metrics.radiusCard
    color: Metrics.card

    Column {
        id: column
        x: 20
        y: 18
        width: parent.width - 40
        spacing: 12

        Row {
            width: parent.width
            spacing: 10

            Rectangle {
                id: badge
                width: 32
                height: 32
                radius: 9
                color: Qt.rgba(card.tint.r, card.tint.g, card.tint.b, 0.13)

                StoreIcon {
                    anchors.centerIn: parent
                    name: card.iconName
                    size: 18
                    strokeWidth: 2
                    color: card.tint
                }
            }

            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - badge.width - 10
                text: card.title
                size: 15
                font.weight: Font.Bold
            }
        }

        Repeater {
            model: card.rows

            Row {
                id: line
                required property var modelData
                width: column.width
                spacing: 10

                StoreIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: line.modelData.tick ? "tick" : (Metrics.reachIcons[line.modelData.icon] || "shield")
                    size: 16
                    color: Metrics.inkSecondary
                }

                UI.Label {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 26
                    text: line.modelData.label
                    size: 13.5
                }
            }
        }

        UI.Label {
            visible: card.text.length > 0
            width: parent.width
            text: card.text
            size: 13.5
            lineHeight: 1.45
            color: Metrics.inkSecondary
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }
    }
}
