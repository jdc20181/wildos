import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// A key's passphrase: saving it in the keyring (checked against the key
// first), or giving the key a new one (ssh-keygen asks for the old one).
UI.Dialog {
    id: dialog

    property string keyPath
    // "save" or "change"
    property string mode: "save"
    property bool busy: false

    signal notice(string text)

    function ask(path, m) {
        keyPath = path;
        mode = m;
        const k = Keychain.key(path);
        title = m === "save" ? qsTr("Save the passphrase of %1").arg(k.label)
            : k.encrypted ? qsTr("Change the passphrase of %1").arg(k.label) : qsTr("Add a passphrase to %1").arg(k.label);
        text = m === "save" ? qsTr("It's checked against the key, then kept in your keyring. Remote adds the key to your key agent when you connect, so you're not asked.")
            : k.encrypted ? qsTr("You're asked for the current one first.")
            : qsTr("Anyone who gets a copy of the key file then needs the passphrase too.");
        first.text = "";
        second.text = "";
        busy = false;
        open();
        first.forceActiveFocus();
    }

    UI.TextField {
        id: first
        Layout.fillWidth: true
        revealable: true
        placeholderText: dialog.mode === "save" ? qsTr("Passphrase") : qsTr("New passphrase")
        onAccepted: if (dialog.mode === "save") go.clicked()
    }
    UI.TextField {
        id: second
        visible: dialog.mode === "change"
        Layout.fillWidth: true
        revealable: true
        placeholderText: qsTr("Type it again")
        error: text.length > 0 && text !== first.text
    }

    buttons: [
        UI.Button {
            text: qsTr("Cancel")
            onClicked: dialog.close()
        },
        UI.Button {
            id: go
            kind: "primary"
            busy: dialog.busy
            text: dialog.mode === "save" ? qsTr("Save") : qsTr("Change")
            enabled: !dialog.busy && first.text.length > 0 && (dialog.mode === "save" || first.text === second.text)
            onClicked: {
                dialog.busy = true;
                const done = (ok, message) => {
                    dialog.busy = false;
                    first.text = "";
                    second.text = "";
                    if (ok) {
                        dialog.close();
                    }
                    dialog.notice(ok ? (dialog.mode === "save" ? qsTr("Passphrase saved in your keyring") : qsTr("Passphrase changed")) : message);
                };
                if (dialog.mode === "save") {
                    Keychain.savePassphrase(dialog.keyPath, first.text, done);
                } else {
                    Keychain.changePassphrase(dialog.keyPath, first.text, done);
                }
            }
        }
    ]
}
