import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// Saved commands, the same list the terminal's snippets panel runs from.
Item {
    id: page

    signal editRequested(var snippet)

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 36
        anchors.rightMargin: 36
        anchors.topMargin: 22
        anchors.bottomMargin: 26
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                UI.Label {
                    text: qsTr("Snippets")
                    size: 26
                    font.weight: Font.Bold
                }
            }
            UI.Button {
                size: "small"
                kind: "primary"
                iconName: "plus"
                text: qsTr("New snippet")
                onClicked: page.editRequested({})
            }
        }
        UI.Label {
            Layout.fillWidth: true
            text: qsTr("Commands you run often, ready in the terminal's side panel, and offered as you type. They stay on this PC.")
            size: 14
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: 900
            spacing: 10
            clip: true
            model: Snippets.snippets
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                id: row
                required property var modelData
                width: ListView.view.width
                height: 66
                radius: 12
                color: rowMouse.containsMouse ? UI.Style.tone(0.07) : UI.Style.tone(0.045)
                border.width: 1
                border.color: UI.Style.tone(0.07)
                Column {
                    x: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 80
                    spacing: 5
                    Row {
                        spacing: 10
                        UI.Label {
                            text: row.modelData.title
                            size: 15
                            font.weight: Font.DemiBold
                        }
                        UI.Label {
                            visible: (row.modelData.note || "").length > 0
                            anchors.baseline: parent.children[0].baseline
                            text: row.modelData.note || ""
                            size: 13
                            color: UI.Style.inkSubtle
                        }
                    }
                    UI.Label {
                        width: parent.width
                        text: row.modelData.command
                        size: 13
                        font.family: "JetBrains Mono"
                        color: UI.Style.inkMuted
                    }
                }
                UI.IconButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "pencil"
                    tip: qsTr("Edit")
                    onClicked: page.editRequested(row.modelData)
                }
                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    anchors.rightMargin: 56
                    hoverEnabled: true
                    onDoubleClicked: page.editRequested(row.modelData)
                }
            }
            footer: Column {
                visible: Snippets.snippets.length === 0
                width: ListView.view.width
                topPadding: 40
                UI.Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("No snippets yet")
                    size: 18
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
