import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A row of What's included: an 18px icon, the part's name over what it
// holds and its size, and the switch. Rows after the first draw the
// hairline above themselves.
Item {
    id: row

    property string iconName
    property string title
    property string text
    property color textColor: UI.Style.inkMuted
    property alias checked: toggle.checked
    property bool locked: false
    property string lockedReason

    signal toggled(bool on)

    implicitHeight: layout.implicitHeight + 24

    Rectangle {
        width: parent.width
        height: 1
        color: BackupStyle.hairline
    }

    RowLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        BackupIcon {
            name: row.iconName
            size: 18
            color: UI.Style.inkMuted
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            UI.Label {
                Layout.fillWidth: true
                text: row.title
                size: 13.5
                elide: Text.ElideRight
            }
            UI.Label {
                Layout.fillWidth: true
                text: row.text
                size: 12.5
                color: row.textColor
                wrapMode: Text.Wrap
            }
        }
        UI.Toggle {
            id: toggle
            locked: row.locked
            lockedReason: row.lockedReason
            Accessible.name: row.title
            onToggled: row.toggled(checked)
        }
    }
}
