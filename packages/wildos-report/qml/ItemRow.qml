import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// One thing the report can hold (ReportReview.dc): an 18px tick box, its
// 17px icon, the name and what it holds, and View, which shows it in the
// preview. A hairline above each.
Item {
    id: row

    property string title
    property string subtitle
    property string iconName
    property bool included
    property bool viewing
    signal toggled(bool on)
    signal viewClicked()

    implicitHeight: Math.max(53, column.implicitHeight + 20)

    Rectangle {
        width: parent.width
        height: 1
        color: Look.hairline
    }

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 1
        anchors.leftMargin: -8
        anchors.rightMargin: -8
        radius: 6
        color: row.viewing ? UI.Style.tone(0.03) : "transparent"
    }

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: 1
        spacing: 12

        Rectangle {
            id: box
            width: 18
            height: 18
            radius: 5
            color: row.included ? UI.Style.accent : "transparent"
            border.width: row.included ? 0 : 1.5
            border.color: UI.Style.tone(0.55)
            Accessible.role: Accessible.CheckBox
            Accessible.name: row.title
            Accessible.checked: row.included
            activeFocusOnTab: true
            Keys.onSpacePressed: row.toggled(!row.included)

            ReportIcon {
                anchors.centerIn: parent
                visible: row.included
                name: "tick"
                size: 13
                strokeWidth: 3
                color: UI.Style.textOnAccent
            }
            UI.FocusRing {
                visible: box.activeFocus
                targetRadius: 5
            }
        }

        ReportIcon {
            name: row.iconName
            size: 17
            strokeWidth: 1.8
            color: UI.Style.inkMuted
        }

        ColumnLayout {
            id: column
            Layout.fillWidth: true
            spacing: 2

            UI.Label {
                Layout.fillWidth: true
                text: row.title
                size: 13.5
            }
            UI.Label {
                Layout.fillWidth: true
                text: row.subtitle
                size: 12
                color: UI.Style.inkMuted
            }
        }

        UI.LinkButton {
            text: qsTr("View")
            size: 13
            onClicked: row.viewClicked()
        }
    }

    // The tick box and the words both tick and untick, as in any list of
    // choices; View keeps its own click.
    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: column.x + column.width
        cursorShape: Qt.PointingHandCursor
        onClicked: row.toggled(!row.included)
    }
}
