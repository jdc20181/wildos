import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A card on Home: a heading, a line of context and whatever the card shows,
// with a link at the bottom to the full page.
UI.Card {
    id: card

    property string title
    property string caption
    property string iconName
    property string linkText
    default property alias body: bodyColumn.data

    signal linkClicked()

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.preferredWidth: 1
    padding: 18
    // The link's focus ring sits just outside it.
    clipContent: false

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            UI.Icon {
                visible: card.iconName.length > 0
                name: card.iconName
                size: 20
                strokeWidth: 1.8
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                UI.Label {
                    Layout.fillWidth: true
                    text: card.title
                    size: 15
                    font.weight: Font.DemiBold
                }

                UI.Label {
                    Layout.fillWidth: true
                    visible: card.caption.length > 0
                    text: card.caption
                    size: 12
                    color: UI.Style.inkMuted
                }
            }
        }

        ColumnLayout {
            id: bodyColumn
            Layout.fillWidth: true
            spacing: 10
        }

        Item {
            Layout.fillHeight: true
        }

        UI.LinkButton {
            visible: card.linkText.length > 0
            Layout.leftMargin: -4
            text: card.linkText
            onClicked: card.linkClicked()
        }
    }
}
