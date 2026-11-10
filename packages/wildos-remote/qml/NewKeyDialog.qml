import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// A new SSH key: Ed25519, made on this PC by ssh-keygen and kept in
// ~/.ssh. The passphrase is what protects it if the file is ever copied;
// the keyring keeps it only when asked.
UI.Dialog {
    id: dialog

    signal notice(string text)

    title: qsTr("New key")
    text: qsTr("An Ed25519 key, made on this PC and kept in your .ssh folder, where ssh and git find it too.")
    size: "medium"

    onAboutToShow: {
        label.text = "";
        passphrase.text = "";
        again.text = "";
        save.checked = true;
        busy = false;
        label.forceActiveFocus();
    }
    property bool busy: false
    readonly property bool matches: passphrase.text === again.text

    UI.Label {
        text: qsTr("Name")
        size: 13
        font.weight: Font.DemiBold
    }
    UI.TextField {
        id: label
        Layout.fillWidth: true
        placeholderText: qsTr("Wild OS laptop")
    }
    UI.Label {
        Layout.topMargin: 6
        text: qsTr("Passphrase")
        size: 13
        font.weight: Font.DemiBold
    }
    UI.TextField {
        id: passphrase
        Layout.fillWidth: true
        revealable: true
        placeholderText: qsTr("Passphrase")
    }
    UI.TextField {
        id: again
        Layout.fillWidth: true
        revealable: true
        placeholderText: qsTr("Type it again")
        error: again.text.length > 0 && !dialog.matches
    }
    UI.Label {
        Layout.fillWidth: true
        text: passphrase.text.length === 0
            ? qsTr("Without a passphrase, anyone who gets a copy of the key file can use it.")
            : qsTr("Use something you'll remember: a lost passphrase can't be recovered.")
        size: 12
        color: passphrase.text.length === 0 ? UI.Style.warning : UI.Style.inkSubtle
        wrapMode: Text.Wrap
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4
        spacing: 12
        enabled: passphrase.text.length > 0
        opacity: enabled ? 1 : 0.5
        UI.Toggle {
            id: save
            checked: true
            Accessible.name: saveLabel.text
        }
        UI.Label {
            id: saveLabel
            Layout.fillWidth: true
            text: qsTr("Save the passphrase in my keyring, so I'm not asked each time")
            size: 13
            wrapMode: Text.Wrap
        }
    }

    buttons: [
        UI.Button {
            text: qsTr("Cancel")
            onClicked: dialog.close()
        },
        UI.Button {
            kind: "primary"
            text: qsTr("Make key")
            busy: dialog.busy
            enabled: dialog.matches && !dialog.busy
            onClicked: {
                dialog.busy = true;
                Keychain.generate(label.text, passphrase.text, save.checked && passphrase.text.length > 0, (ok, message) => {
                    dialog.busy = false;
                    passphrase.text = "";
                    again.text = "";
                    if (ok) {
                        dialog.close();
                    }
                    dialog.notice(ok ? (message || qsTr("Key made")) : message);
                });
            }
        }
    ]
}
