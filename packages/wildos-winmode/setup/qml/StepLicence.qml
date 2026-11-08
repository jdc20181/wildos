import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI
import WildOS.WinMode.Backend

// 9.2 (WinModeHome.dc) when the licence is for Windows 11 Home, and 9.3
// (WinModeNoLicence.dc) when there is none to use.
Page {
    id: page

    // home, none or inuse
    property string kind: "none"

    // licence: firmware, key or none; homeChoice: upgrade or home
    signal chosen(string licence, string key, string homeChoice)
    signal skipped()
    signal back()

    readonly property bool home: kind === "home"

    title: "Set up Windows mode"
    text: home ? "Windows apps can run on this PC. How smoothly depends on the edition of Windows it came with."
               : "Windows programs can still work on this PC. Choose how you'd like to handle the Windows licence."
    note: home ? "You can switch from Home to Pro later without setting anything up again."
               : "Your choice only affects Windows mode. The rest of Wild OS is free."

    Banner {
        tone: page.home ? "accent" : "warning"
        title: page.home ? "Windows 11 Home found" : page.kind === "inuse" ? "Windows is still installed on this PC" : "No Windows licence built into this PC"
        text: page.home ? "Your licence works with Windows mode. Pro has a feature built in that shows each app as a normal window; Home doesn't, so on Home we use our own method instead."
            : page.kind === "inuse" ? "Its licence covers one copy of Windows at a time, and that copy is still on this PC. A key from another licence works for Windows mode."
            : "Common on self-built PCs and older machines. A key from a previous PC often still works."
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        visible: page.home
        Accessible.role: Accessible.Grouping
        Accessible.name: "Windows edition"

        RadioCard {
            id: upgrade
            checked: true
            title: "Upgrade to Windows 11 Pro"
            chip: "Recommended"
            description: "Fully supported. Apps open quickly and look and behave like the rest of your desktop. Microsoft sells the upgrade, and it uses the licence you already have."

            UI.LinkButton {
                text: "See the price on Microsoft's site"
                onClicked: Setup.openUrl("https://www.microsoft.com/en-gb/d/windows-11-pro/dg7gmgf0d7fv")
            }
        }

        RadioCard {
            id: useHome
            enabled: false
            title: "Use Windows 11 Home"
            chip: "Experimental"
            chipTone: "warning"
            description: "Free with your licence and works for most apps, but it's still being developed and isn't supported yet. Apps can be slower to draw, and some, including parts of Office, may look or behave oddly."
            reason: "Not ready yet. Upgrade to Pro, or skip for now."
        }

        RadioCard {
            id: skipHome
            title: "Skip Windows mode for now"
            description: "Many Windows programs still run on the desktop without it, and Office works in the browser."
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        visible: !page.home
        Accessible.role: Accessible.Grouping
        Accessible.name: "Windows licence"

        RadioCard {
            id: withKey
            checked: true
            title: "Enter a product key"
            description: "From a sticker, an email or a licence you bought for another PC."

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
                    onClicked: Setup.openUrl("https://www.microsoft.com/en-gb/d/windows-11-home/dg7gmgf0krt0")
                }
            }

            UI.Label {
                visible: keyField.error
                text: "That isn't the right shape. A key has 25 letters and numbers in five groups."
                size: 12
                color: UI.Style.danger
            }
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
            enabled: page.home || !withKey.checked || Setup.keyShapeValid(keyField.text)
            onClicked: {
                if (page.home) {
                    if (skipHome.checked) {
                        page.skipped();
                    } else {
                        page.chosen("firmware", "", upgrade.checked ? "upgrade" : "home");
                    }
                } else if (skipNone.checked) {
                    page.skipped();
                } else if (noKey.checked) {
                    page.chosen("none", "", "");
                } else {
                    page.chosen("key", Setup.normaliseKey(keyField.text), "");
                }
            }
        }
    ]
}
