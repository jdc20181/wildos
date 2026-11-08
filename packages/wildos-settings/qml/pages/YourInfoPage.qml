import QtQuick
import QtCore
import QtQuick.Dialogs
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Your info, after Windows 11: the account picture large, the name and
// account type, then changing the name and choosing a new picture. Both are
// saved through AccountsService, so the sign-in screen, Start and the lock
// screen show them too.
PageScroll {
    id: page

    Connections {
        target: Users
        function onDone(what, message) {
            if (what === "picture" || what === "name") {
                // The navigation and Start read the account through System.
                System.userChanged();
            }
        }
        function onFailed(what, reason) {
            if (what === "picture" || what === "name") {
                notice.show(what === "picture" ? "The picture was not changed" : "The name was not changed", reason, true);
            }
        }
    }

    Notice {
        id: notice
    }

    UI.Card {
        Layout.fillWidth: true
        implicitHeight: 176

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            spacing: 28

            UI.Avatar {
                size: 120
                source: System.userAvatar
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                UI.Label {
                    Layout.fillWidth: true
                    text: System.userFullName
                    size: 22
                    font.weight: Font.DemiBold
                }

                UI.Label {
                    Layout.fillWidth: true
                    text: "Local account, " + System.userName
                    color: UI.Style.inkMuted
                }

                UI.Label {
                    Layout.fillWidth: true
                    text: Users.me.admin ? "Administrator" : "Standard user"
                    color: UI.Style.accentText
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Adjust your info"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "pencil"
                    title: "Your name"
                    description: "Shown on the sign-in screen, in Start and to anyone who shares this computer"

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Change name"
                        enabled: Users.accountsService
                        onClicked: nameDialog.open()
                    }
                }

                UI.SettingRow {
                    iconName: "image"
                    title: "Choose a picture"
                    description: "Any photo works. You choose the part that goes in the circle."

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Browse files"
                        onClicked: fileDialog.open()
                    }
                }

                UI.SettingRow {
                    visible: (Users.me.icon || "").length > 0
                    iconName: "restore"
                    title: "Use the Wild OS picture"
                    description: "Go back to the picture every new account starts with"

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Use it"
                        onClicked: Users.clearPicture()
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Related settings"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "lock"
                    title: "Sign-in options"
                    description: "Password, automatic sign-in and screen lock"
                    chevron: true
                    onClicked: Navigator.open("signin")
                }

                UI.SettingRow {
                    iconName: "people"
                    title: "Other users"
                    description: "Add the people who share this computer"
                    chevron: true
                    onClicked: Navigator.open("otherusers")
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "Choose a picture"
        nameFilters: ["Pictures (*.jpg *.jpeg *.png *.webp *.bmp *.gif)"]
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        onAccepted: pictureDialog.choose(selectedFile)
    }

    PictureDialog {
        id: pictureDialog
    }

    UI.Dialog {
        id: nameDialog
        title: "Change your name"
        text: "Use the name you would like people to see. It does not change the name you sign in with."
        onAboutToShow: {
            nameField.text = System.userFullName;
            nameField.selectAll();
            nameField.forceActiveFocus();
        }

        UI.TextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: "Your name"
            error: /[:,]/.test(text)
            onAccepted: nameSave.clicked()
        }

        UI.Label {
            Layout.fillWidth: true
            visible: nameField.error
            text: "A name can't contain a colon or comma."
            size: 12
            color: UI.Style.danger
        }

        buttons: [
            UI.Button {
                id: nameSave
                text: "Save"
                kind: "primary"
                enabled: nameField.text.trim().length > 0 && !nameField.error
                onClicked: {
                    if (!enabled) {
                        return;
                    }
                    Users.setRealName(nameField.text);
                    nameDialog.close();
                }
            },
            UI.Button {
                text: "Cancel"
                onClicked: nameDialog.close()
            }
        ]
    }
}
