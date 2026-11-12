import QtQuick
import QtQuick.Layouts
import WildOS.Mail
import WildOS.UI as UI

// Calendar in a window of its own, from right-clicking Calendar on the
// rail and choosing Open in new window. Normally it's a tab of Mail.
UI.AppWindow {
    id: window

    width: 1384
    height: 802
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: qsTr("Calendar")

    function showEvent(key, day) {
        page.showEvent(key, day);
    }

    Shortcut {
        sequence: "Ctrl+E"
        onActivated: search.forceActiveFocus()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: window.maximised ? 0 : 1
        spacing: 0

        UI.TitleBar {
            Layout.fillWidth: true
            title: qsTr("Calendar")
            tinted: true
            frame: window.frame

            Item {
                anchors.fill: parent
                UI.SearchField {
                    id: search
                    anchors.centerIn: parent
                    width: Math.min(520, parent.width - 160)
                    height: 32
                    placeholderText: qsTr("Search events")
                    onTextChanged: searchTimer.restart()
                    onEscapePressed: {
                        text = "";
                        page.clearSearch();
                    }
                    onAccepted: {
                        searchTimer.stop();
                        page.search(text);
                    }
                }
                Timer {
                    id: searchTimer
                    interval: 300
                    onTriggered: page.search(search.text)
                }
                CommandButton {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    iconOnly: true
                    iconName: "settings"
                    tip: qsTr("Settings")
                    onClicked: page.toggleSettings()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            AppRail {
                Layout.fillHeight: true
                current: "calendar"
                squareCorner: window.maximised
                onPicked: function (app) {
                    if (app !== "calendar") {
                        Apps.open(app);
                    }
                }
            }
            CalendarPage {
                id: page
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
