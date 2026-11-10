import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// The terminal's snippets (RemoteTerminal.dc): saved commands to run with
// one click, a search over them, and whether a click runs in every open
// tab.
Item {
    id: panel

    property var tab: null
    property bool runAll: false
    property string chosen

    signal runRequested(string command)
    signal runAllToggled()

    readonly property var shown: {
        const q = search.text.trim().toLowerCase();
        return Snippets.snippets.filter(s => q.length === 0 || (s.title + " " + s.command).toLowerCase().indexOf(q) >= 0);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            UI.Label {
                Layout.fillWidth: true
                text: qsTr("Snippets")
                size: 16
                font.weight: Font.Bold
            }
            UI.IconButton {
                width: 28
                height: 28
                iconName: "plus"
                iconSize: 15
                iconColor: UI.Style.iconMuted
                tip: qsTr("New snippet")
                focusPolicy: Qt.NoFocus
                onClicked: snippetDialog.edit({})
            }
        }
        UI.TextField {
            id: search
            Layout.fillWidth: true
            height: 32
            leftPadding: 34
            placeholderText: qsTr("Search snippets")
            UI.Icon {
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                name: "search"
                size: 14
                color: UI.Style.iconMuted
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            clip: true
            model: panel.shown
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                id: card
                required property var modelData
                readonly property bool current: panel.chosen === modelData.id
                width: ListView.view.width
                height: 61
                radius: 10
                color: current ? Qt.rgba(143 / 255, 180 / 255, 255 / 255, 0.12) : cardMouse.containsMouse ? UI.Style.tone(0.06) : UI.Style.tone(0.04)
                border.width: 1
                border.color: current ? Qt.rgba(143 / 255, 180 / 255, 255 / 255, 0.5) : UI.Style.tone(0.06)
                Column {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 48
                    spacing: 6
                    UI.Label {
                        width: parent.width
                        text: card.modelData.title
                        size: 14
                        font.weight: Font.DemiBold
                    }
                    UI.Label {
                        width: parent.width
                        text: card.modelData.command
                        size: 12
                        font.family: "JetBrains Mono"
                        color: UI.Style.inkMuted
                    }
                }
                MouseArea {
                    id: cardMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: m => {
                        panel.chosen = card.modelData.id;
                        if (m.button === Qt.RightButton) {
                            snippetDialog.edit(card.modelData);
                        }
                    }
                    onDoubleClicked: panel.runRequested(card.modelData.command)
                }
                UI.IconButton {
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.top: parent.top
                    anchors.topMargin: 6
                    width: 26
                    height: 26
                    glyph: RemoteIcons.play
                    iconSize: 13
                    iconColor: card.current ? UI.Style.accent : UI.Style.iconMuted
                    tip: panel.runAll ? qsTr("Run in all open tabs") : qsTr("Run")
                    focusPolicy: Qt.NoFocus
                    enabled: panel.tab !== null && !panel.tab.ended
                    onClicked: {
                        panel.chosen = card.modelData.id;
                        panel.runRequested(card.modelData.command);
                    }
                }
            }
            footer: UI.Label {
                visible: panel.shown.length === 0
                width: ListView.view.width
                topPadding: 20
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: Snippets.snippets.length === 0 ? qsTr("Save commands you run often, and run them from here.") : qsTr("No snippet matches")
                size: 13
                color: UI.Style.inkMuted
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            radius: 10
            color: UI.Style.tone(0.04)
            border.width: 1
            border.color: UI.Style.tone(0.06)
            UI.Glyph {
                x: 13
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                path: RemoteIcons.broadcast
                color: panel.runAll ? UI.Style.accent : UI.Style.iconMuted
            }
            Column {
                x: 38
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 38 - 64
                spacing: 2
                UI.Label {
                    text: qsTr("Run in all open tabs")
                    size: 13
                    font.weight: Font.DemiBold
                }
                UI.Label {
                    width: parent.width
                    text: qsTr("Asks before running on more than one server")
                    size: 12
                    color: UI.Style.inkMuted
                    wrapMode: Text.Wrap
                }
            }
            UI.Toggle {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                checked: panel.runAll
                focusPolicy: Qt.NoFocus
                Accessible.name: qsTr("Run in all open tabs")
                onToggled: panel.runAllToggled()
            }
        }
    }

    SnippetDialog {
        id: snippetDialog
    }
}
