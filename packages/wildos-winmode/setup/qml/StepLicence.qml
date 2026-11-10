import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI
import WildOS.WinMode.Backend

// 9.3 (WinModeNoLicence.dc) when there is no licence Windows mode can use:
// none built in, one still in use by Windows on this PC, or one for Windows
// 11 Home. Windows mode always installs Pro; a Home key can't activate it.
Page {
    id: page

    // home, none or inuse
    property string kind: "none"

    // licence: key or none; homeChoice is always empty now
    signal chosen(string licence, string key, string homeChoice)
    signal skipped()
    signal back()

    readonly property bool home: kind === "home"

    title: "Set up Windows mode"
    text: "Windows programs can still work on this PC. Choose how you'd like to handle the Windows licence."
    note: "Windows mode needs Windows 11 Pro. A Home key won't activate it."

    Banner {
        tone: "warning"
        title: page.home ? "This PC came with Windows 11 Home" : page.kind === "inuse" ? "Windows is still installed on this PC" : "No Windows licence built into this PC"
        text: page.home ? "Windows mode needs Windows 11 Pro, so this licence can't activate it. Set it up without activating, or with a Pro key."
            : page.kind === "inuse" ? "Its licence is built into this PC. Windows mode can use it as well, or a key from another licence."
            : "Common on self-built PCs and older machines. A key from a previous PC often still works."
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        Accessible.role: Accessible.Grouping
        Accessible.name: "Windows licence"

        RadioCard {
            id: withKey
            checked: true
            title: "Enter a product key"
            description: "A Windows 11 Pro key, from a sticker, an email or a licence you bought for another PC."

            RowLayout {
                spacing: 8

                UI.TextField {
                    id: keyField
                    Layout.preferredWidth: 330
                    placeholderText: "XXXXX-XXXXX-XXXXX-XXXXX-XXXXX"
                    font.letterSpacing: 0.5
                    inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                    maximumLength: 34
                    error: text.length >= 25 && !Setup.keyShapeValid(text)
                    Accessible.name: "Windows product key"
                    onActiveFocusChanged: if (activeFocus) withKey.checked = true
                    onEditingFinished: if (Setup.keyShapeValid(text)) text = Setup.normaliseKey(text)
                }

                UI.LinkButton {
                    text: "Where to buy a licence"
                    onClicked: Setup.openUrl("https://www.microsoft.com/en-gb/d/windows-11-pro/dg7gmgf0d7fv")
                }
            }

            UI.Label {
                visible: keyField.error
                text: "That isn't the right shape. A key has 25 letters and numbers in five groups."
                size: 12
                color: UI.Style.danger
            }
        }

        // The built-in licence while Windows is still installed: the
        // person's call. The installed Windows and its licence are left as
        // they are; Windows mode activates with the same key.
        RadioCard {
            id: thisPc
            visible: page.kind === "inuse" && Setup.edition !== "home"
            title: "Use this PC's licence"
            description: "Windows mode activates with the licence built into this PC. The Windows already installed isn't changed. Microsoft's terms count each copy of Windows, so you may want to remove the old one later."
        }

        RadioCard {
            id: noKey
            title: "Continue without activating"
            description: "Windows and your programs work the same. Microsoft's terms ask you to buy a licence; you can add a key any time in Settings."
        }

        RadioCard {
            id: skipNone
            title: "Skip Windows mode for now"
            description: "Many Windows programs still run on the desktop without it, and Office works in the browser. Turn it on later in Settings."
        }
    }

    buttons: [
        UI.Button {
            text: "Back"
            onClicked: page.back()
        },
        UI.Button {
            kind: "primary"
            text: "Continue"
            enabled: !withKey.checked || Setup.keyShapeValid(keyField.text)
            onClicked: {
                if (skipNone.checked) {
                    page.skipped();
                } else if (thisPc.checked && thisPc.visible) {
                    page.chosen("firmware", "", "");
                } else if (noKey.checked) {
                    page.chosen("none", "", "");
                } else {
                    page.chosen("key", Setup.normaliseKey(keyField.text), "");
                }
            }
        }
    ]
}
