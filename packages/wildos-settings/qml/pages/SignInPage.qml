import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Sign-in options, after Windows 11: changing the password (current one
// first, then the new one twice and an optional hint), signing in
// automatically when the computer starts, restarting apps after signing
// in, and the way to the screen lock. Turning automatic sign-in on asks for
// your own password, so the system can keep the key that opens your saved
// passwords (KWallet) when nobody types one.
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
                autoDialog.close();
                notice.show("Automatic sign-in", message, false);
            } else if (what === "wallet") {
                walletDialog.close();
                notice.show("Saved passwords", message, false);
            }
        }
        function onFailed(what, reason) {
            if (what === "wallet" || what === "wallet-other") {
                walletDialog.needsOther = walletDialog.needsOther || what === "wallet-other";
                walletDialog.error = reason;
            } else if (what === "password") {
                passwordDialog.error = reason;
            } else if (what === "autologin") {
                autoToggle.checked = Qt.binding(() => page.autoOn);
                if (autoDialog.visible) {
                    autoDialog.error = reason;
                } else {
                    notice.show("Automatic sign-in was not changed", reason, true);
                }
            }
        }
    }

    Notice {
        id: notice
    }

    // The saved passwords notification (wildos-setup-wallet) opens this
    // page with Navigator.request "wallet".
    function takeRequest() {
        if (Navigator.request === "wallet") {
            Navigator.request = "";
            walletDialog.open();
        }
    }

    Component.onCompleted: takeRequest()

    Connections {
        target: Navigator
        function onRequestChanged() {
            page.takeRequest();
        }
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
                        : "Anyone who turns the computer on gets straight to your files and saved passwords, so only use it on a computer that stays at home."
                    status: page.autoOn ? "On" : "Off"

                    UI.Toggle {
                        id: autoToggle
                        anchors.verticalCenter: parent.verticalCenter
                        checked: page.autoOn
                        enabled: Users.busy !== "autologin"
                        Accessible.name: "Sign in automatically when this computer starts"
                        onToggled: {
                            if (checked) {
                                checked = Qt.binding(() => page.autoOn);
                                autoDialog.open();
                            } else {
                                Users.setAutoLogin(false);
                            }
                        }
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
        id: walletDialog

        property string error
        property bool needsOther: false

        title: "Open your saved passwords"
        text: needsOther
            ? "Your saved passwords use a different password from your account. Type both, and Wild OS moves them to your account password so they open when you sign in."
            : page.autoOn
                ? "This computer signs you in automatically, so your saved passwords need your password once. After this they open by themselves at every start."
                : "Type your account password so your saved passwords open when you sign in."
        onAboutToShow: {
            error = "";
            needsOther = false;
            walletPasswordField.text = "";
            walletOtherField.text = "";
            walletPasswordField.forceActiveFocus();
        }

        UI.TextField {
            id: walletPasswordField
            Layout.fillWidth: true
            placeholderText: "Your account password"
            revealable: true
            onTextChanged: walletDialog.error = ""
            onAccepted: walletDialog.needsOther ? walletOtherField.forceActiveFocus() : walletButton.clicked()
        }

        UI.TextField {
            id: walletOtherField
            Layout.fillWidth: true
            visible: walletDialog.needsOther
            placeholderText: "The password your saved passwords use"
            revealable: true
            onTextChanged: walletDialog.error = ""
            onAccepted: walletButton.clicked()
        }

        UI.Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: walletDialog.error
            size: 12
            color: UI.Style.danger
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }

        buttons: [
            UI.Button {
                id: walletButton
                text: "Continue"
                kind: "primary"
                busy: Users.busy === "wallet"
                enabled: walletPasswordField.text.length > 0 && (!walletDialog.needsOther || walletOtherField.text.length > 0)
                    && Users.busy !== "wallet"
                onClicked: {
                    if (!enabled) {
                        return;
                    }
                    walletDialog.error = "";
                    Users.openWallet(walletPasswordField.text, walletDialog.needsOther ? walletOtherField.text : "");
                }
            },
            UI.Button {
                text: "Cancel"
                enabled: Users.busy !== "wallet"
                onClicked: walletDialog.close()
            }
        ]
    }

    UI.Dialog {
        id: autoDialog

        property string error

        title: "Sign in automatically"
        text: (Protection.status.encryption === "on"
            ? "Anyone who gets past the disk password when this computer starts will reach your files and your saved passwords, without your account password."
            : "Anyone who turns this computer on will reach your files and your saved passwords, without a password.")
            + " Type your password so Wild OS can open your saved passwords for you."
        onAboutToShow: {
            error = "";
            autoPasswordField.text = "";
            autoPasswordField.forceActiveFocus();
        }

        UI.TextField {
            id: autoPasswordField
            Layout.fillWidth: true
            placeholderText: "Your password"
            revealable: true
            onTextChanged: autoDialog.error = ""
            onAccepted: turnOnButton.clicked()
        }

        UI.Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: autoDialog.error
            size: 12
            color: UI.Style.danger
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }

        buttons: [
            UI.Button {
                id: turnOnButton
                text: "Turn on"
                kind: "primary"
                busy: Users.busy === "autologin"
                enabled: autoPasswordField.text.length > 0 && Users.busy !== "autologin"
                onClicked: {
                    if (!enabled) {
                        return;
                    }
                    autoDialog.error = "";
                    Users.setAutoLogin(true, autoPasswordField.text);
                }
            },
            UI.Button {
                text: "Cancel"
                enabled: Users.busy !== "autologin"
                onClicked: autoDialog.close()
            }
        ]
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

        StrengthLine {
            Layout.fillWidth: true
            password: newField.text
            oldPassword: currentField.text
            names: [Users.me.userName || "", Users.me.realName || ""]
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
