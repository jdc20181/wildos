import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// A server's password prompt, from the terminal's ssh (through AskPass),
// laid out as the desktop sign-in is. The password can be saved in the
// keyring, and is only once the server has accepted it.
Panel {
    id: dialog

    property string askId
    property var details: ({})
    property bool answered: false
    readonly property bool retry: details.retry === true

    width: 460
    padding: 28

    function ask(id, d) {
        askId = id;
        details = d;
        password.text = "";
        // A saved password that didn't work is replaced by the one typed now.
        save.checked = d.saved === true;
        answered = false;
        open();
        password.forceActiveFocus();
    }
    function closeFor(id) {
        if (id === askId) {
            answered = true;
            close();
        }
    }

    onClosed: {
        if (!answered) {
            AskPass.cancel(askId);
        }
        password.text = "";
    }

    contentItem: ColumnLayout {
        spacing: 14

        RowLayout {
            spacing: 14
            Image {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                source: "qrc:/qt/qml/WildOS/Remote/data/org.wildos.remote.svg"
                sourceSize: Qt.size(80, 80)
            }
            ColumnLayout {
                spacing: 2
                UI.Label {
                    text: qsTr("Sign in to %1").arg(dialog.details.name || "")
                    size: 20
                    font.weight: Font.Bold
                }
                UI.Label {
                    text: dialog.details.address || ""
                    size: 13
                    color: UI.Style.inkMuted
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            radius: 10
            color: dialog.retry ? Qt.rgba(255 / 255, 154 / 255, 138 / 255, 0.10) : UI.Style.tone(0.05)
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 12
                UI.Spinner {
                    visible: !dialog.retry
                    size: 22
                    running: dialog.visible
                }
                UI.Icon {
                    visible: dialog.retry
                    name: "warning"
                    size: 18
                    color: "#ff9a8a"
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    UI.Label {
                        Layout.fillWidth: true
                        text: dialog.retry ? qsTr("%1 didn't accept that").arg(dialog.details.name || "") : qsTr("Connected to %1").arg(dialog.details.name || "")
                        size: 13
                        font.weight: Font.DemiBold
                    }
                    UI.Label {
                        Layout.fillWidth: true
                        text: dialog.details.savedFailed ? qsTr("The password saved in your keyring didn't work. Type the current one.")
                            : dialog.retry ? qsTr("That didn't work. Check the password, then try again.")
                            : qsTr("Its identity checks out. Waiting for you to sign in")
                        size: 12
                        color: UI.Style.inkMuted
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        UI.Label {
            text: qsTr("Password")
            size: 13
            font.weight: Font.DemiBold
        }
        UI.TextField {
            id: password
            Layout.fillWidth: true
            size: "large"
            leftPadding: 40
            revealable: true
            placeholderText: qsTr("Password")
            onAccepted: go.clicked()
            UI.Icon {
                x: 14
                anchors.verticalCenter: parent.verticalCenter
                name: "lock"
                size: 15
                color: UI.Style.iconMuted
            }
        }
        RowLayout {
            spacing: 12
            UI.Toggle {
                id: save
                Accessible.name: saveLabel.text
            }
            UI.Label {
                id: saveLabel
                text: qsTr("Save in my keyring")
                size: 14
            }
        }
        UI.Label {
            Layout.fillWidth: true
            text: qsTr("Your keyring is locked with your Wild OS password, so saved passwords are only readable while you're signed in. Nothing is saved until the server accepts it.")
            size: 12
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
            lineHeight: 1.35
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 10
            Item { Layout.fillWidth: true }
            UI.Button {
                size: "large"
                text: qsTr("Cancel")
                onClicked: dialog.close()
            }
            UI.Button {
                id: go
                size: "large"
                kind: "primary"
                text: qsTr("Sign in")
                enabled: password.text.length > 0
                onClicked: {
                    dialog.answered = true;
                    AskPass.answer(dialog.askId, password.text, save.checked);
                    dialog.close();
                }
            }
        }
    }
}
