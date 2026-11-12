import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// One of the three small cards under the status (AppBackup.dc): an 18px
// icon, a 13.5px title, a line under it, and for the drive a 200px bar of
// how full it is.
Panel {
    id: card

    property string iconName
    property string title
    property string text
    // 0 to 1, or below 0 for no bar.
    property real fill: -1

    implicitHeight: row.implicitHeight + padding * 2

    RowLayout {
        id: row
        width: parent.width
        spacing: 12

        BackupIcon {
            Layout.alignment: Qt.AlignTop
            name: card.iconName
            size: 18
            color: UI.Style.inkMuted
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            UI.Label {
                Layout.fillWidth: true
                text: card.title
                size: 13.5
                font.weight: UI.Style.weightSemiBold
                elide: Text.ElideRight
            }
            UI.Label {
                Layout.fillWidth: true
                text: card.text
                size: 12.5
                color: UI.Style.inkMuted
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
            Rectangle {
                visible: card.fill >= 0
                Layout.topMargin: 6
                Layout.preferredWidth: Math.min(200, parent.width)
                Layout.preferredHeight: 5
                radius: 2.5
                color: BackupStyle.track

                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1, card.fill))
                    height: parent.height
                    radius: 2.5
                    color: card.fill > 0.9 ? BackupStyle.late : UI.Style.accent
                }
            }
        }
    }
}
