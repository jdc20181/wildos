import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A card on Home, in the design's three parts: a header (icon, title, one
// line, and a link or control on the right), a fixed set of 30px rows, and
// a last line pinned to the bottom so it lines up with the card beside it.
// Cards never grow to fit more and never scroll inside: the grid gives each
// row of cards one height, and `minimumHeight` is what this card needs.
UI.Card {
    id: card

    property string title
    property string caption
    property string iconName
    // The link on the right of the header; leave empty to put a control
    // there with `trailing`.
    property string linkText
    property int headerPadding: 11
    default property alias body: bodyColumn.data
    property alias trailing: trailingRow.data
    property alias footer: footerColumn.data

    readonly property int minimumHeight: Math.ceil(layout.implicitHeight) + 2

    signal linkClicked()

    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.preferredWidth: 1
    // Rows paint their hover to the card's rounded edge.
    clipContent: true

    ColumnLayout {
        id: layout
        anchors.fill: parent
        spacing: 0

        RowLayout {
            id: header
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.topMargin: card.headerPadding
            Layout.bottomMargin: card.headerPadding
            spacing: 14

            UI.Icon {
                visible: card.iconName.length > 0
                name: card.iconName
                size: 20
                strokeWidth: 1.8
                color: UI.Style.inkMuted
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                UI.Label {
                    Layout.fillWidth: true
                    text: card.title
                    size: 14.5
                    font.weight: UI.Style.weightSemiBold
                }

                UI.Label {
                    Layout.fillWidth: true
                    visible: card.caption.length > 0
                    text: card.caption
                    size: 12.5
                    color: UI.Style.inkMuted
                }
            }

            UI.LinkButton {
                visible: card.linkText.length > 0
                Layout.rightMargin: -4
                text: card.linkText
                onClicked: card.linkClicked()
            }

            Row {
                id: trailingRow
                spacing: 8
            }
        }

        ColumnLayout {
            id: bodyColumn
            Layout.fillWidth: true
            spacing: 0
        }

        Item {
            Layout.fillHeight: true
        }

        ColumnLayout {
            id: footerColumn
            Layout.fillWidth: true
            spacing: 0
        }
    }
}
