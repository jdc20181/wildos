import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// Every port forward, from every SSH host. Each one is open while its
// host's terminal is, on this PC only (127.0.0.1).
Item {
    id: page

    signal editHost(string id)

    readonly property var rows: {
        const out = [];
        for (const h of Hosts.hosts) {
            if (h.kind !== "terminal") {
                continue;
            }
            for (const f of (h.ssh.forwards || [])) {
                out.push({ host: h, forward: f });
            }
        }
        return out;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 36
        anchors.rightMargin: 36
        anchors.topMargin: 22
        anchors.bottomMargin: 26
        spacing: 14

        UI.Label {
            Layout.preferredHeight: 44
            text: qsTr("Port forwarding")
            size: 26
            font.weight: UI.Style.weightBold
        }
        UI.Label {
            Layout.fillWidth: true
            Layout.maximumWidth: 900
            text: qsTr("A forward makes a port on a server, or on a machine the server can reach, answer on this PC. It's open while that host's terminal is, and only programs on this PC can use it. Add one from a host's settings.")
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
            model: page.rows
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                id: row
                required property var modelData
                width: ListView.view.width
                height: 62
                radius: 12
                color: UI.Style.surfaceRaised
                border.width: 1
                border.color: UI.Style.borderSubtle
                UI.Glyph {
                    x: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18
                    height: 18
                    path: RemoteIcons.forward
                    color: UI.Style.iconInk
                }
                Column {
                    x: 50
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    UI.Label {
                        text: qsTr("localhost:%1 to %2:%3").arg(row.modelData.forward.localPort).arg(row.modelData.forward.remoteHost || "localhost").arg(row.modelData.forward.remotePort)
                        size: 14
                        font.family: "JetBrains Mono"
                        font.weight: UI.Style.weightSemiBold
                    }
                    UI.Label {
                        text: qsTr("Through %1").arg(row.modelData.host.name)
                        size: 13
                        color: UI.Style.inkMuted
                    }
                }
                UI.Button {
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    size: "small"
                    text: qsTr("Edit host")
                    onClicked: page.editHost(row.modelData.host.id)
                }
            }
            footer: UI.Label {
                visible: page.rows.length === 0
                width: ListView.view.width
                topPadding: 40
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No forwards yet")
                size: 18
                font.weight: UI.Style.weightSemiBold
            }
        }
    }
}
