import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// The two facts under the description, whether the app runs well and who
// made it, each with a tinted round badge (WinApp.dc.html).
UI.Card {
    id: card

    property string iconName: "checkmark"
    // "good", "warning", "danger" or "neutral": the badge's colour.
    property string tone: "good"
    property string title
    property string description

    readonly property color badgeInk: tone === "good" ? UI.Style.accentText
        : tone === "warning" ? UI.Style.warning
        : tone === "danger" ? UI.Style.danger
        : UI.Style.iconInk

    leftPadding: 17
    rightPadding: 17
    topPadding: 15
    bottomPadding: 15

    contentItem: RowLayout {
        spacing: 12

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 30
            implicitHeight: 30
            radius: 15
            color: card.tone === "neutral" ? UI.Style.tone(0.10) : card.tone === "good" ? UI.Style.accentSelected : Qt.alpha(card.badgeInk, 0.18)

            UI.Icon {
                anchors.centerIn: parent
                name: card.iconName
                size: 15
                strokeWidth: 2.6
                color: card.badgeInk
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            UI.Label {
                Layout.fillWidth: true
                text: card.title
                font.weight: Font.DemiBold
            }

            UI.Label {
                Layout.fillWidth: true
                text: card.description
                size: 12
                color: UI.Style.inkMuted
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                maximumLineCount: 3
            }
        }
    }
}
