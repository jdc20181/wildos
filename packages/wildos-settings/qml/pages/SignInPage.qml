import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Sign-in options, after Windows 11: changing the password (current one
// first, then the new one twice and an optional hint), signing in
// automatically when the computer starts, restarting apps after signing
// in, and the way to the screen lock.
PageScroll {
    id: page

    readonly property bool autoOn: Users.autoLogin.length > 0 && Users.autoLogin === Users.me.userName
    readonly property bool autoOther: Users.autoLogin.length > 0 && Users.autoLogin !== Users.me.userName

    // ksmserver's loginMode, as KDE's session settings write it. It saves
    // the open apps at sign-out only in restorePreviousLogout; a session
    // someone saved by hand (restoreSavedSession) also counts as on.
    property int sessionRevision: 0
    readonly property bool restoreApps: {
        sessionRevision;
        return System.config("ksmserverrc", "General", "loginMode", "restorePreviousLogout") !== "emptySession";
    }

    function setRestoreApps(on) {
        System.setConfig("ksmserverrc", "General", "loginMode", on ? "restorePreviousLogout" : "emptySession");
        sessionRevision++;
    }

    Connections {
        target: Users
        function onDone(what, message) {
            if (what === "password") {
                if (message.length > 0) {
                    passwordDialog.close();
                    notice.show("Password changed", message, false);
                }
            } else if (what === "autologin") {
                notice.show("Automatic sign-in", message, false);
            }
        }
        function onFailed(what, reason) {
            if (what === "password") {
                passwordDialog.error = reason;
            } else if (what === "autologin") {
                notice.show("Automatic sign-in was not changed", reason, true);
                autoToggle.checked = Qt.binding(() => page.autoOn);
            }
        }
    }

    Notice {
        id: notice
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Ways to sign in"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "lock"
                    title: "Password"
                    description: "Sign in with your account's password"

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Change"
                        onClicked: passwordDialog.open()
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Additional settings"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "power"
                    minHeight: 72
                    title: "Sign in automatically when this computer starts"
                    description: page.autoOther ? "Set up for " + Users.autoLogin + ". Turning it on here moves it to you."
                        : "Anyone who turns the computer on gets straight to your files, so only use it on a computer that stays at home."
                    status: page.autoOn ? "On" : "Off"

                    UI.Toggle {
                        id: autoToggle
                        anchors.verticalCenter: parent.verticalCenter
                        checked: page.autoOn
                        enabled: Users.busy !== "autologin"
                        Accessible.name: "Sign in automatically when this computer starts"
                        onToggled: Users.setAutoLogin(checked)
                    }
                }

                UI.SettingRow {
                    iconName: "restart"
                    minHeight: 72
                    title: "Restart my apps when I sign back in"
                    description: "Apps that were open when you signed out or restarted open again, where the app supports it"
                    status: page.restoreApps ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: page.restoreApps
                        Accessible.name: "Restart my apps when I sign back in"
                        onToggled: page.setRestoreApps(checked)
                    }
                }

                UI.SettingRow {
                    iconName: "clock"
                    title: "Screen lock"
                    description: "Lock after a while and ask for your password on wake"
                    chevron: true
                    onClicked: Navigator.open("screenlock")
                }
            }
        }
    }

    UI.Dialog {
        id: passwordDialog

        property string error
        readonly property string problem: {
            if (newField.text.length > 0 && newField.text.length < 8) {
                return "Use at least 8 characters.";
            }
            if (confirmField.text.length > 0 && confirmField.text !== newField.text) {
                return "The two new passwords don't match.";
            }
            if (newField.text.length > 0 && newField.text === currentField.text) {
                return "The new password is the same as the current one.";
            }
            return "";
        }
        readonly property bool ready: currentField.text.length > 0 && newField.text.length >= 8
            && confirmField.text === newField.text && problem.length === 0

        title: "Change your password"
        text: Protection.status.encryption === "on"
            ? "Type your current password, then the new one twice. The disk itself still unlocks with the password it was given when Wild OS was installed."
            : "Type your current password, then the new one twice."
        onAboutToShow: {
            error = "";
            currentField.text = "";
            newField.text = "";
            confirmField.text = "";
            hintField.text = Users.me.hint || "";
            currentField.forceActiveFocus();
        }

        UI.TextField {
            id: currentField
            Layout.fillWidth: true
            placeholderText: "Current password"
            revealable: true
            onTextChanged: passwordDialog.error = ""
            onAccepted: newField.forceActiveFocus()
        }

        UI.TextField {
            id: newField
            Layout.fillWidth: true
            Layout.topMargin: 6
            placeholderText: "New password"
            revealable: true
            error: text.length > 0 && text.length < 8
            onTextChanged: passwordDialog.error = ""
            onAccepted: confirmField.forceActiveFocus()
        }

        UI.TextField {
            id: confirmField
            Layout.fillWidth: true
            placeholderText: "Confirm the new password"
            revealable: true
            error: text.length > 0 && text !== newField.text
            onAccepted: hintField.forceActiveFocus()
        }

        UI.TextField {
            id: hintField
            Layout.fillWidth: true
            Layout.topMargin: 6
            placeholderText: "Password hint (optional)"
            onAccepted: changeButton.clicked()
        }

        UI.Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: passwordDialog.error.length > 0 ? passwordDialog.error : passwordDialog.problem
            size: 12
            color: UI.Style.danger
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }

        buttons: [
            UI.Button {
                id: changeButton
                text: "Change"
                kind: "primary"
                busy: Users.busy === "password"
                enabled: passwordDialog.ready && Users.busy !== "password"
                onClicked: {
                    if (!enabled) {
                        return;
                    }
                    passwordDialog.error = "";
                    Users.changePassword(currentField.text, newField.text, hintField.text);
                }
            },
            UI.Button {
                text: "Cancel"
                enabled: Users.busy !== "password"
                onClicked: passwordDialog.close()
            }
        ]
    }
}
