import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// A question from ssh, ssh-keygen or ssh-add (through AskPass): a
// passphrase, a password, a yes or no, or "touch your security key", which
// stays up until the key is touched.
UI.Dialog {
    id: dialog

    property string askId
    property string kind: "passphrase"
    property bool answered: false

    function ask(id, heading, prompt, k) {
        askId = id;
        kind = k;
        answered = false;
        title = k === "touch" ? qsTr("Touch your security key")
            : k === "confirm" ? (heading || qsTr("Are you sure?"))
            : heading || (k === "pin" ? qsTr("Enter your security key's PIN") : qsTr("Enter the passphrase"));
        // ssh's own prompt, minus the colon it ends with.
        text = prompt.replace(/:\s*$/, "");
        field.text = "";
        open();
        if (k !== "touch" && k !== "confirm") {
            field.forceActiveFocus();
        }
    }
    function closeFor(id) {
        if (id === askId) {
            answered = true;
            close();
        }
    }

    onClosed: if (!answered) AskPass.cancel(askId)

    UI.TextField {
        id: field
        visible: dialog.kind !== "touch" && dialog.kind !== "confirm"
        Layout.fillWidth: true
        revealable: true
        placeholderText: dialog.kind === "pin" ? qsTr("PIN") : dialog.kind === "password" ? qsTr("Password") : qsTr("Passphrase")
        onAccepted: okButton.clicked()
    }
    UI.Spinner {
        visible: dialog.kind === "touch"
        Layout.alignment: Qt.AlignHCenter
        size: 22
    }

    buttons: [
        UI.Button {
            text: qsTr("Cancel")
            visible: dialog.kind !== "touch"
            onClicked: dialog.close()
        },
        UI.Button {
            id: okButton
            kind: "primary"
            visible: dialog.kind !== "touch"
            text: dialog.kind === "confirm" ? qsTr("Yes") : qsTr("OK")
            onClicked: {
                dialog.answered = true;
                AskPass.answer(dialog.askId, field.text);
                field.text = "";
                dialog.close();
            }
        }
    ]
}
