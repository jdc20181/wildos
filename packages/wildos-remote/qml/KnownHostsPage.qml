import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// ~/.ssh/known_hosts, the identities this PC has seen, shared with every
// other SSH program. Names are stored hashed, so a name shows only when it
// matches one of your hosts.
Item {
    id: page

    signal notice(string text)

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 36
        anchors.rightMargin: 36
        anchors.topMargin: 22
        anchors.bottomMargin: 26
        spacing: 14

        UI.Label {
            Layout.preferredHeight: 44
            text: qsTr("Known hosts")
            size: 26
            font.weight: Font.Bold
        }
        UI.Label {
            Layout.fillWidth: true
            Layout.maximumWidth: 900
            text: qsTr("Each server's identity, from the first time you connected. Remote won't connect to a server whose identity has changed without asking you. Removing one means the next connection asks again, as if it were new.")
            size: 14
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
            lineHeight: 1.35
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: 900
            spacing: 10
            clip: true
            model: HostKeys.known
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                id: row
                required property var modelData
                width: ListView.view.width
                height: 62
                radius: 12
                color: UI.Style.tone(0.045)
                border.width: 1
                border.color: UI.Style.tone(0.07)
                UI.Glyph {
                    x: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18
                    height: 18
                    path: RemoteIcons.knownHosts
                    color: row.modelData.marker === "@revoked" ? UI.Style.danger : UI.Style.iconMuted
                }
                Column {
                    x: 50
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 170
                    spacing: 4
                    UI.Label {
                        width: parent.width
                        text: row.modelData.name.length > 0 ? row.modelData.name
                            : row.modelData.hashed ? qsTr("A server you haven't saved (its name is hashed)") : qsTr("Unnamed")
                        size: 14
                        font.weight: Font.DemiBold
                        color: row.modelData.name.length > 0 ? UI.Style.ink : UI.Style.inkMuted
                    }
                    UI.Label {
                        width: parent.width
                        text: row.modelData.keyType + "  " + row.modelData.fingerprint + (row.modelData.seen ? "  ·  " + qsTr("added %1").arg(row.modelData.seen) : "")
                        size: 12
                        font.family: "JetBrains Mono"
                        color: UI.Style.inkSubtle
                    }
                }
                UI.Button {
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    size: "small"
                    text: qsTr("Remove")
                    onClicked: {
                        if (!HostKeys.forget(row.modelData.index)) {
                            page.notice(qsTr("Your known hosts file couldn't be changed."));
                        }
                    }
                }
            }
            footer: UI.Label {
                visible: HostKeys.known.length === 0
                width: ListView.view.width
                topPadding: 40
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No known hosts yet")
                size: 18
                font.weight: Font.DemiBold
            }
        }
    }
}
