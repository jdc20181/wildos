import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// The first connection to a server or PC (RemoteCheck.dc): nothing vouches
// for it yet, so its fingerprint is shown to check against the one the
// server shows its own administrator.
//
// For SSH the key goes into known_hosts when the person connects: ssh only
// connects to servers whose key is there, and that is what lets Remote
// warn if it ever changes. For a desktop the person can choose to remember
// the certificate (FreeRDP keeps it) or trust it for this session only.
Panel {
    id: dialog

    property var host: ({})
    property var result: ({})
    // "ssh" or "rdp"
    property string kind: "ssh"
    property bool remember: true

    signal accepted(var host, var result)
    signal declined()

    width: 520
    padding: 28

    function ask(h, r) {
        host = h;
        result = r;
        kind = "ssh";
        remember = true;
        open();
    }
    function askCertificate(h, r) {
        host = h;
        result = r;
        kind = "rdp";
        remember = true;
        open();
    }

    readonly property var rows: kind === "ssh" ? [
        { label: qsTr("Server"), value: result.name || "" },
        { label: qsTr("Key type"), value: result.keyType || "" },
        { label: qsTr("Fingerprint"), value: result.fingerprint || "", wide: true }
    ] : [
        { label: qsTr("Computer"), value: result.host || "" },
        { label: qsTr("Calls itself"), value: result.commonName || qsTr("No name given") },
        { label: qsTr("Identity issued by"), value: result.selfSigned ? qsTr("Itself (normal for most PCs)") : (result.issuer || "") },
        { label: qsTr("Valid until"), value: result.validUntil || "", colour: result.expired ? UI.Style.danger : "" },
        { label: qsTr("Fingerprint"), value: result.fingerprint || "", wide: true }
    ]

    contentItem: ColumnLayout {
        spacing: 16

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            radius: 14
            color: UI.Style.warningSoft
            UI.Icon {
                anchors.centerIn: parent
                name: "shield"
                size: 26
                strokeWidth: 1.8
                color: UI.Style.warning
            }
        }
        UI.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: dialog.kind === "ssh" ? qsTr("Is this the right server?") : qsTr("Is this the right computer?")
            size: 20
            font.weight: UI.Style.weightBold
        }
        UI.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("This is your first time connecting to %1, so Remote can't vouch for it yet. If you can, check the fingerprint below matches the one on the %2.")
                .arg(dialog.host.name || "").arg(dialog.kind === "ssh" ? qsTr("server") : qsTr("PC"))
            size: 14
            color: UI.Style.inkStatus
            wrapMode: Text.Wrap
            lineHeight: 1.4
        }
        UI.Label {
            visible: dialog.kind === "rdp" && dialog.result.mismatch === true
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: dialog.result.commonName
                ? qsTr("Its certificate is made out to %1, not to the address you typed.").arg(dialog.result.commonName)
                : qsTr("Its certificate doesn't name any computer, so only the fingerprint can say it's the right one.")
            size: 13
            color: UI.Style.warning
            wrapMode: Text.Wrap
        }
        InfoRows {
            Layout.fillWidth: true
            rows: dialog.rows
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            UI.Toggle {
                visible: dialog.kind === "rdp"
                checked: dialog.remember
                onToggled: dialog.remember = checked
                Accessible.name: rememberLabel.text
            }
            UI.Label {
                id: rememberLabel
                Layout.fillWidth: true
                text: dialog.kind === "rdp" ? qsTr("Remember this computer and warn me if it ever changes")
                    : qsTr("Connecting adds it to your known hosts, so Remote can warn you if it ever changes.")
                size: dialog.kind === "rdp" ? 14 : 13
                color: dialog.kind === "rdp" ? UI.Style.ink : UI.Style.inkMuted
                wrapMode: Text.Wrap
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Item { Layout.fillWidth: true }
            UI.Button {
                size: "large"
                text: qsTr("Cancel")
                onClicked: dialog.close()
            }
            UI.Button {
                size: "large"
                kind: "primary"
                text: qsTr("Connect")
                onClicked: {
                    dialog.answered = true;
                    dialog.close();
                    dialog.accepted(dialog.host, Object.assign({}, dialog.result, { remember: dialog.remember }));
                }
            }
        }
    }

    // Cancel, Escape or anything else that closes it is a no.
    property bool answered: false
    onAboutToShow: answered = false
    onClosed: if (!answered) declined()
}
