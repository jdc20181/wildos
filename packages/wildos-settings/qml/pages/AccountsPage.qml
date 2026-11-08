import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Accounts, after Windows 11: who is signed in, then Your info, Sign-in
// options and Other users.
PageScroll {
    id: page

    UI.Card {
        Layout.fillWidth: true
        implicitHeight: 112

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 20

            UI.Avatar {
                size: 72
                source: System.userAvatar
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                UI.Label {
                    Layout.fillWidth: true
                    text: System.userFullName
                    size: 18
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
                    size: 12
                    color: UI.Style.accentText
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Account settings"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "account"
                    title: "Your info"
                    description: "Your name and picture"
                    chevron: true
                    onClicked: Navigator.open("users")
                }

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
                    description: Users.others.length === 0 ? "Add the people who share this computer"
                        : Format.plural(Users.others.length, "other person uses", "other people use") + " this computer"
                    chevron: true
                    onClicked: Navigator.open("otherusers")
                }
            }
        }
    }
}
