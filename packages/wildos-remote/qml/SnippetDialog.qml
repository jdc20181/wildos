import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// A snippet: a name, the command, and a short note shown with it.
UI.Dialog {
    id: dialog

    property var snippet: ({})

    function edit(s) {
        snippet = s || {};
        title = snippet.id ? qsTr("Edit snippet") : qsTr("New snippet");
        name.text = snippet.title || "";
        command.text = snippet.command || "";
        note.text = snippet.note || "";
        open();
        name.forceActiveFocus();
    }

    UI.TextField {
        id: name
        Layout.fillWidth: true
        placeholderText: qsTr("Name, like Reload nginx safely")
    }
    UI.TextField {
        id: command
        Layout.fillWidth: true
        font.family: "JetBrains Mono"
        placeholderText: "sudo nginx -t && sudo systemctl reload nginx"
    }
    UI.TextField {
        id: note
        Layout.fillWidth: true
        placeholderText: qsTr("Note, like tests the config first")
    }

    buttons: [
        UI.Button {
            visible: dialog.snippet.id !== undefined
            danger: true
            text: qsTr("Delete")
            onClicked: {
                Snippets.remove(dialog.snippet.id);
                dialog.close();
            }
        },
        UI.Button {
            text: qsTr("Cancel")
            onClicked: dialog.close()
        },
        UI.Button {
            kind: "primary"
            text: qsTr("Save")
            enabled: name.text.trim().length > 0 && command.text.trim().length > 0
            onClicked: {
                Snippets.save({ id: dialog.snippet.id || "", title: name.text.trim(), command: command.text.trim(), note: note.text.trim() });
                dialog.close();
            }
        }
    ]
}
