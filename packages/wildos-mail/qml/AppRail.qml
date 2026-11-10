import QtQuick
import WildOS.Mail
import WildOS.UI as UI

// The rail down the left of Mail and Calendar (MailMicrosoft.dc,
// CalendarMicrosoft.dc): each is a tab of the same window, and
// right-clicking one opens it in a window of its own. People and Tasks
// join it when they are built.
Rectangle {
    id: rail

    property string current: "mail"
    property bool squareCorner: false

    signal picked(string app)

    implicitWidth: 60
    color: UI.Style.shade(0.30, 0.04)
    bottomLeftRadius: squareCorner ? 0 : UI.Style.windowRadius - 1

    Column {
        x: 4
        y: 8
        spacing: 4

        Repeater {
            model: [
                { id: "mail", name: qsTr("Mail"), icon: "mail" },
                { id: "calendar", name: qsTr("Calendar"), icon: "calendar" }
            ]
            delegate: Rectangle {
                id: item
                required property var modelData
                readonly property bool active: rail.current === modelData.id
                width: 52
                height: 52
                radius: 8
                color: active ? UI.Style.accentSelected : mouse.containsMouse ? UI.Style.surfaceHover : "transparent"
                Accessible.role: Accessible.Button
                Accessible.name: modelData.name

                Rectangle {
                    visible: item.active
                    x: 0
                    y: 10
                    width: 3
                    height: 32
                    radius: 2
                    color: UI.Style.accent
                }
                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    MailIcon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        name: item.modelData.icon
                        size: 19
                        color: item.active ? UI.Style.accent : UI.Style.iconMuted
                    }
                    UI.Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: item.modelData.name
                        size: 10
                        font.weight: item.active ? Font.DemiBold : Font.Normal
                        color: item.active ? UI.Style.accent : UI.Style.inkMuted
                    }
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: item.active ? Qt.ArrowCursor : Qt.PointingHandCursor
                    onClicked: function (event) {
                        if (event.button === Qt.RightButton && item.modelData.id === "calendar") {
                            railMenu.app = item.modelData.id;
                            railMenu.popup();
                        } else if (!item.active) {
                            rail.picked(item.modelData.id);
                        }
                    }
                }
            }
        }
    }

    UI.Menu {
        id: railMenu
        property string app
        entries: [
            { text: qsTr("Open in new window"), icon: "external", run: () => Apps.openWindow(railMenu.app) }
        ]
    }
}
