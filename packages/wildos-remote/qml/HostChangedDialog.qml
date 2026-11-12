import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// The server isn't the one Remote knew (RemoteHostChanged.dc). Not
// connecting is the big button; trusting the new identity is the small red
// one, and nothing changes in known_hosts (or FreeRDP's store) unless it is
// pressed.
Panel {
    id: dialog

    property var host: ({})
    property var result: ({})
    property string kind: "ssh"

    signal rebuilt(var host, var result)
    signal refused()

    width: 500
    padding: 28

    function ask(h, r) {
        host = h;
        result = r;
        kind = "ssh";
        open();
    }
    function askCertificate(h, r) {
        host = h;
        result = r;
        kind = "rdp";
        open();
    }

    function shortFingerprint(fp) {
        if (!fp) {
            return "";
        }
        // SHA256:Kd7f…pR2a, as the board shows it; the whole of each is in
        // its tooltip.
        const body = fp.startsWith("SHA256:") ? fp.substring(7) : fp.replace(/:/g, "");
        return (fp.startsWith("SHA256:") ? "SHA256:" : "") + body.substring(0, 4) + "…" + body.substring(body.length - 4);
    }
    readonly property var previous: (result.previous || [])[0] || {}

    contentItem: ColumnLayout {
        spacing: 16

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            radius: 14
            color: Qt.alpha(UI.Style.danger, 0.16)
            UI.Icon {
                anchors.centerIn: parent
                name: "shield-alert"
                size: 26
                strokeWidth: 1.8
                color: UI.Style.danger
            }
        }
        UI.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: dialog.kind === "ssh" ? qsTr("%1 isn't the same server").arg(dialog.host.name || "") : qsTr("%1 isn't the same computer").arg(dialog.host.name || "")
            size: 20
            font.weight: UI.Style.weightBold
            wrapMode: Text.Wrap
        }
        UI.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: dialog.kind === "ssh"
                ? qsTr("Its identity has changed since you last connected. That's normal after a server is rebuilt or reinstalled. If nobody did that, someone could be listening in, so don't connect.")
                : qsTr("Its identity has changed since you last connected. That's normal after Windows is reinstalled or its certificate is renewed. If nobody did that, someone could be listening in, so don't connect.")
            size: 14
            color: UI.Style.inkStatus
            wrapMode: Text.Wrap
            lineHeight: 1.4
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: compare.implicitHeight + 8
            radius: 12
            color: UI.Style.surfaceRest
            border.width: 1
            border.color: UI.Style.borderSubtle
            GridLayout {
                id: compare
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                columns: 3
                rowSpacing: 0
                columnSpacing: 16

                UI.Label { Layout.preferredHeight: 40; text: qsTr("Last time"); size: 14; color: UI.Style.inkMuted }
                UI.Label {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    text: dialog.shortFingerprint(dialog.previous.fingerprint)
                    size: 13
                    font.family: "JetBrains Mono"
                    font.weight: UI.Style.weightSemiBold
                    UI.Tooltip { text: dialog.previous.fingerprint || ""; visible: lastMouse.containsMouse }
                    MouseArea { id: lastMouse; anchors.fill: parent; hoverEnabled: true }
                }
                UI.Label {
                    Layout.preferredHeight: 40
                    text: dialog.previous.seen ? qsTr("Seen %1").arg(dialog.previous.seen) : ""
                    size: 13
                    color: UI.Style.inkMuted
                }
                UI.Label { Layout.preferredHeight: 40; text: qsTr("Now"); size: 14; color: UI.Style.inkMuted }
                UI.Label {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    text: dialog.shortFingerprint(dialog.result.fingerprint)
                    size: 13
                    font.family: "JetBrains Mono"
                    font.weight: UI.Style.weightSemiBold
                    color: UI.Style.danger
                    UI.Tooltip { text: dialog.result.fingerprint || ""; visible: nowMouse.containsMouse }
                    MouseArea { id: nowMouse; anchors.fill: parent; hoverEnabled: true }
                }
                UI.Label {
                    Layout.preferredHeight: 40
                    text: qsTr("Different")
                    size: 13
                    color: UI.Style.danger
                }
            }
        }
        UI.Button {
            Layout.fillWidth: true
            size: "large"
            kind: "primary"
            text: qsTr("Don't connect")
            focus: true
            onClicked: dialog.close()
        }
        UI.Label {
            id: trustLink
            Layout.alignment: Qt.AlignHCenter
            text: dialog.kind === "ssh" ? qsTr("It was rebuilt, trust the new identity") : qsTr("It was reinstalled, trust the new identity")
            size: 14
            font.weight: UI.Style.weightSemiBold
            font.underline: trustMouse.containsMouse || trustLink.activeFocus
            color: UI.Style.danger
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: text
            Keys.onReturnPressed: trustMouse.clicked(null)
            Keys.onSpacePressed: trustMouse.clicked(null)
            MouseArea {
                id: trustMouse
                anchors.fill: parent
                anchors.margins: -6
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    dialog.answered = true;
                    dialog.close();
                    dialog.rebuilt(dialog.host, dialog.result);
                }
            }
        }
    }

    property bool answered: false
    onAboutToShow: answered = false
    onClosed: if (!answered) refused()
}
