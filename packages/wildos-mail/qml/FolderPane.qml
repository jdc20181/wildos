import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts
import WildOS.Mail
import WildOS.UI as UI

// The folder pane (MailMicrosoft.dc, 236px): Favourites, then each account
// with its folders, "Create new folder", and what Mail kept out this week.
Item {
    id: pane

    signal addAccount()
    signal createFolder(string accountId)
    signal openPrivacy()
    signal folderMenu(string accountId, string fullName, string name, string role)
    signal crossAccount()

    function iconFor(role) {
        switch (role) {
        case "inbox": return "inbox";
        case "sent": return "sent";
        case "drafts": return "drafts";
        case "archive": return "archive";
        case "trash": return "trash";
        case "junk": return "junk";
        case "outbox": return "outbox";
        }
        return "folder";
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 6
        spacing: 0

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: MailEngine.folders
            boundsBehavior: Flickable.StopAtBounds
            QQC.ScrollBar.vertical: UI.ScrollBar {}

            delegate: Item {
                id: row

                required property int index
                required property string kind
                required property string accountId
                required property string fullName
                required property string name
                required property string folderRole
                required property int depth
                required property int count
                required property bool expanded

                readonly property bool current: kind === "folder" && !MailEngine.searching
                    && (accountId === "" ? MailEngine.folderAccount === "" && MailEngine.folderRole === fullName
                                         : MailEngine.folderAccount === accountId && MailEngine.folderName === fullName)

                width: list.width
                height: kind === "header" ? 36 : kind === "create" ? 30 : 32

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    anchors.topMargin: row.kind === "header" ? 4 : 0
                    radius: 6
                    visible: row.kind === "folder"
                    color: drop.containsDrag ? UI.Style.accentSelected : row.current ? UI.Style.accentSelected : mouse.containsMouse ? UI.Style.surfaceHover : "transparent"
                    border.width: drop.containsDrag ? 1 : 0
                    border.color: UI.Style.accent
                }

                // Messages dragged from the list move here (MessageRowItem).
                DropArea {
                    id: drop
                    anchors.fill: parent
                    enabled: row.kind === "folder" && row.accountId.length > 0 && row.folderRole !== "outbox"
                    keys: ["wildos-mail-messages"]
                    onDropped: function (event) {
                        const keys = event.source.keys || [];
                        const here = keys.filter(k => k.split("\x1f")[0] === row.accountId);
                        if (here.length < keys.length) {
                            pane.crossAccount();
                        }
                        if (here.length === 0) {
                            return;
                        }
                        if (row.folderRole === "trash") {
                            MailEngine.removeKeys(here);
                        } else if (row.folderRole === "junk") {
                            MailEngine.junkKeys(here);
                        } else {
                            MailEngine.moveKeys(here, row.fullName);
                        }
                        event.accept();
                    }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function (event) {
                        if (event.button === Qt.RightButton) {
                            if (row.kind === "folder" && row.accountId.length > 0 && row.folderRole !== "outbox") {
                                pane.folderMenu(row.accountId, row.fullName, row.name, row.folderRole);
                            }
                            return;
                        }
                        if (row.kind === "header") {
                            MailEngine.toggleExpanded(row.accountId);
                        } else if (row.kind === "create") {
                            pane.createFolder(row.accountId);
                        } else {
                            MailEngine.openFolder(row.accountId, row.fullName);
                        }
                    }
                }

                // Account or Favourites heading.
                RowLayout {
                    visible: row.kind === "header"
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 4
                    spacing: 8

                    MailIcon {
                        name: row.expanded ? "chevron-down" : "chevron-right"
                        size: 13
                        color: UI.Style.inkMuted
                    }
                    UI.Label {
                        Layout.fillWidth: true
                        text: row.name
                        size: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    visible: row.kind === "folder"
                    anchors.fill: parent
                    anchors.leftMargin: 16 + 18 + Math.max(0, row.depth - 1) * 14
                    anchors.rightMargin: 16
                    spacing: 10

                    MailIcon {
                        name: pane.iconFor(row.folderRole)
                        size: 16
                        color: row.current ? UI.Style.accent : UI.Style.iconInk
                    }
                    UI.Label {
                        Layout.fillWidth: true
                        text: row.name
                        size: 13
                        font.weight: row.current ? Font.DemiBold : Font.Normal
                        elide: Text.ElideRight
                    }
                    UI.Label {
                        visible: row.count > 0
                        text: row.count
                        size: 12
                        font.weight: Font.DemiBold
                        color: UI.Style.accent
                    }
                }

                UI.Label {
                    visible: row.kind === "create"
                    anchors.verticalCenter: parent.verticalCenter
                    x: 44
                    text: row.name
                    size: 13
                    color: mouse.containsMouse ? UI.Style.accentHover : UI.Style.accent
                }
            }

            footer: Item {
                width: list.width
                height: 38

                MouseArea {
                    id: addMouse
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: pane.addAccount()
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    spacing: 8

                    MailIcon {
                        name: "plus"
                        size: 15
                        color: UI.Style.accent
                    }
                    UI.Label {
                        text: qsTr("Add account")
                        size: 13
                        color: addMouse.containsMouse ? UI.Style.accentHover : UI.Style.accent
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // What Mail kept away from senders this week.
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.bottomMargin: 12
            Layout.topMargin: 8
            implicitHeight: trackers.implicitHeight + 22
            radius: 10
            color: UI.Style.surfaceRest
            border.width: 1
            border.color: UI.Style.borderSubtle

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: pane.openPrivacy()
            }

            RowLayout {
                id: trackers
                anchors.fill: parent
                anchors.margins: 11
                anchors.leftMargin: 12
                spacing: 10

                MailIcon {
                    name: "shield-check"
                    size: 18
                    color: UI.Style.success
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    UI.Label {
                        Layout.fillWidth: true
                        text: MailEngine.trackersThisWeek === 1 ? qsTr("1 tracker blocked") : qsTr("%1 trackers blocked").arg(MailEngine.trackersThisWeek)
                        size: 12
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    UI.Label {
                        Layout.fillWidth: true
                        text: qsTr("This week, across all mail")
                        size: 11
                        color: UI.Style.inkMuted
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
