import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// One row of a status list (9.1, 9.5, 9.6): a 22px circle, a 14px name
// over a 12px muted line, and a 12px value at the right; 56px tall,
// 10 / 18 padding, a hairline between rows.
//
// state: ok (green tick), done (accent tick), active (accent ring and
// dot), pending (empty ring), warning, failed, checking.
Item {
    id: row

    property string state: "pending"
    property string title
    property string detail
    property string value
    property bool divider: y > 0

    Layout.fillWidth: true
    implicitHeight: Math.max(56, column.implicitHeight + 20)
    Accessible.role: Accessible.ListItem
    Accessible.name: title + ", " + detail + (value.length > 0 ? ", " + value : "")

    Rectangle {
        visible: row.divider
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: UI.Style.divider
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 14

        Rectangle {
            implicitWidth: 22
            implicitHeight: 22
            radius: 11
            readonly property bool filled: row.state === "ok" || row.state === "done" || row.state === "warning" || row.state === "failed"
            color: row.state === "ok" ? Qt.rgba(UI.Style.success.r, UI.Style.success.g, UI.Style.success.b, 0.2)
                 : row.state === "done" ? UI.Style.accent
                 : row.state === "warning" ? UI.Style.warningSoft
                 : row.state === "failed" ? Qt.rgba(UI.Style.danger.r, UI.Style.danger.g, UI.Style.danger.b, 0.18)
                 : "transparent"
            border.width: filled ? 0 : 2
            border.color: row.state === "active" ? UI.Style.accent : UI.Style.tone(0.22)

            UI.Icon {
                anchors.centerIn: parent
                visible: row.state === "ok" || row.state === "done"
                name: "checkmark"
                size: 12
                strokeWidth: row.state === "done" ? 3.2 : 3
                color: row.state === "done" ? UI.Style.textOnAccent : UI.Style.success
            }

            UI.Icon {
                anchors.centerIn: parent
                visible: row.state === "warning" || row.state === "failed"
                name: row.state === "failed" ? "close" : "exclamation"
                size: 12
                strokeWidth: 3
                color: row.state === "failed" ? UI.Style.danger : UI.Style.warning
            }

            Rectangle {
                anchors.centerIn: parent
                visible: row.state === "active"
                width: 8
                height: 8
                radius: 4
                color: UI.Style.accent
            }

            UI.Spinner {
                anchors.centerIn: parent
                visible: row.state === "checking"
                size: 16
            }
        }

        ColumnLayout {
            id: column
            Layout.fillWidth: true
            spacing: 2

            UI.Label {
                Layout.fillWidth: true
                text: row.title
                size: 14
            }

            UI.Label {
                Layout.fillWidth: true
                visible: row.detail.length > 0
                text: row.detail
                size: 12
                color: UI.Style.inkMuted
                wrapMode: Text.Wrap
                elide: Text.ElideNone
            }
        }

        UI.Label {
            visible: row.value.length > 0
            text: row.value
            size: 12
            color: UI.Style.inkMuted
        }
    }
}
