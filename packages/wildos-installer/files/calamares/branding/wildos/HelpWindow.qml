/* The installer's help, shipped with it so it works offline: the live
 * system may have no internet. One window, opened on a topic by "Get help",
 * "Help choosing" and each "Show me how" on the Before we start page.
 */
import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import WildOS.UI as UI

Window {
    id: help

    property string topic: "general"

    function openOn(name) {
        topic = name in pages ? name : "general";
        x = screen.virtualX + Math.round((screen.width - width) / 2);
        y = screen.virtualY + Math.round((screen.height - height) / 2);
        show();
        raise();
        requestActivate();
    }

    readonly property var pages: ({
        "general": {
            title: qsTr("Help with installing"),
            intro: qsTr("Installing takes about 10 minutes. Nothing on your disk changes until you press Install on the last step, so you can look around first."),
            sections: [
                { t: qsTr("Before we start"), b: qsTr("Checks this computer. Amber cards are Windows settings to change in Windows before Wild OS can sit beside it. Replacing Windows needs none of them.") },
                { t: qsTr("Keyboard"), b: qsTr("Pick the layout printed on your keys and try it in the box. It matters for the disk password, which you type before anything else loads.") },
                { t: qsTr("Where to install"), b: qsTr("Keep Windows and share the disk, replace it, or set up partitions yourself.") },
                { t: qsTr("Encryption"), b: qsTr("Your disk password locks everything when the computer is off. The recovery key is a spare way in, in case you forget the password.") },
                { t: qsTr("Afterwards"), b: qsTr("When the computer restarts, Wild OS asks for your name and account, Wi-Fi and where you are, and can bring your files across from Windows.") }
            ],
            steps: []
        },
        "choosing": {
            title: qsTr("Which one is right for me?"),
            intro: "",
            sections: [
                { t: qsTr("Install alongside Windows"), b: qsTr("Best if you still need Windows now and then. Both stay on the disk, and each time you switch on you pick which one starts. Windows keeps its files, and the space you give Wild OS comes from Windows' free space.") },
                { t: qsTr("Replace Windows"), b: qsTr("Best if you are done with Windows. Everything on the disk is erased, so copy your files to a USB drive or online storage first. Wild OS gets the whole disk.") },
                { t: qsTr("Something else"), b: qsTr("For people who know partitions: decide what each part of the disk is for, its size and whether it is encrypted.") }
            ],
            steps: []
        },
        "bitlocker": {
            title: qsTr("Pausing BitLocker in Windows"),
            intro: qsTr("BitLocker is Windows' own drive encryption. While it is on, nothing outside Windows can safely make the Windows drive smaller, which is how Wild OS makes room. Suspending it lasts until Windows next restarts, then Windows turns it back on by itself."),
            sections: [
                { t: qsTr("Before you start"), b: qsTr("Make sure you know your Microsoft account password or have your BitLocker recovery key. Windows may ask for it.") }
            ],
            steps: [
                qsTr("Restart the computer and start Windows."),
                qsTr("Open Settings, then Privacy and security, then Device encryption. On some editions, search the Start menu for \"Manage BitLocker\" instead."),
                qsTr("Choose Suspend protection (or Turn off device encryption) and confirm."),
                qsTr("Restart from Windows straight back into this installer, then press Check again.")
            ]
        },
        "faststartup": {
            title: qsTr("Turning off Fast Startup in Windows"),
            intro: qsTr("Fast Startup is a Windows setting. When you shut down, Windows does not really close: it saves itself to the disk and leaves the Windows drive in use. Changing that drive from another system can then lose or damage files. With Fast Startup off, Windows starts a few seconds slower and shuts down properly."),
            sections: [],
            steps: [
                qsTr("Start Windows and open Control Panel, then Hardware and Sound, then Power Options."),
                qsTr("Choose \"Choose what the power buttons do\" on the left."),
                qsTr("Choose \"Change settings that are currently unavailable\"."),
                qsTr("Untick \"Turn on fast startup\" and press Save changes."),
                qsTr("Shut down Windows, start this installer again and press Check again.")
            ]
        },
        "dirty": {
            title: qsTr("Shutting Windows down fully"),
            intro: qsTr("Windows did not finish shutting down last time, so its drive is marked as still in use. Shutting it down properly clears that."),
            sections: [],
            steps: [
                qsTr("Start Windows and sign in."),
                qsTr("Hold the Shift key while you choose Shut down in the Start menu."),
                qsTr("Start this installer again and press Check again.")
            ]
        },
        "space": {
            title: qsTr("Making room in Windows"),
            intro: qsTr("Wild OS needs 40 GB, taken from the free space on the Windows drive. Windows keeps what it already uses."),
            sections: [],
            steps: [
                qsTr("Start Windows and open Settings, then System, then Storage."),
                qsTr("Use Cleanup recommendations, empty the Recycle Bin, and move big files such as videos to another drive."),
                qsTr("When there is at least 45 GB free, start this installer again and press Check again.")
            ]
        },
        "encryption": {
            title: qsTr("About the disk password"),
            intro: qsTr("Wild OS encrypts its part of the disk. Without the disk password or the recovery key, what is on it is unreadable, even to someone who takes the disk out."),
            sections: [
                { t: qsTr("The recovery key"), b: qsTr("A second way to unlock the disk. Save it to a USB stick or write it down at the end of the install, and keep it away from this computer.") },
                { t: qsTr("Not encrypting"), b: qsTr("Starts a little faster, but anyone who has your computer can read your files.") }
            ],
            steps: []
        }
    })
    readonly property var page: pages[topic] || pages["general"]

    width: 560
    height: Math.min(660, body.implicitHeight + 116)
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.ApplicationModal
    color: "transparent"
    title: page.title

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: help.close()
    }

    Rectangle {
        anchors.fill: parent
        radius: UI.Style.radiusDialog
        color: "#222428"
        border.width: 1
        border.color: UI.Style.borderDialog

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: 32
            anchors.bottomMargin: 84
            contentHeight: body.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: body
                width: flick.width
                spacing: 14

                Text {
                    Layout.fillWidth: true
                    text: help.page.title
                    color: UI.Style.ink
                    font.family: UI.Style.fontFamily
                    font.letterSpacing: UI.Style.tracking(18)
                    font.pixelSize: 18
                    font.weight: UI.Style.weightSemiBold
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: help.page.intro
                    color: UI.Style.inkStatus
                    font.family: UI.Style.fontFamily
                    font.letterSpacing: UI.Style.tracking(13)
                    font.pixelSize: 13
                    lineHeight: 1.4
                    wrapMode: Text.WordWrap
                }
                Repeater {
                    model: help.page.steps
                    delegate: RowLayout {
                        required property string modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: 12
                        Rectangle {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22
                            Layout.alignment: Qt.AlignTop
                            radius: 11
                            color: UI.Style.accentChip
                            Text {
                                anchors.centerIn: parent
                                text: index + 1
                                color: UI.Style.accentText
                                font.family: UI.Style.fontFamily
                                font.letterSpacing: UI.Style.tracking(11)
                                font.pixelSize: 11
                                font.weight: UI.Style.weightBold
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData
                            color: UI.Style.ink
                            font.family: UI.Style.fontFamily
                            font.letterSpacing: UI.Style.tracking(13)
                            font.pixelSize: 13
                            lineHeight: 1.4
                            wrapMode: Text.WordWrap
                        }
                    }
                }
                Repeater {
                    model: help.page.sections
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 3
                        Text {
                            text: modelData.t
                            color: UI.Style.ink
                            font.family: UI.Style.fontFamily
                            font.letterSpacing: UI.Style.tracking(14)
                            font.pixelSize: 14
                            font.weight: UI.Style.weightSemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.b
                            color: UI.Style.inkStatus
                            font.family: UI.Style.fontFamily
                            font.letterSpacing: UI.Style.tracking(13)
                            font.pixelSize: 13
                            lineHeight: 1.4
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        WButton {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 24
            primary: true
            text: qsTr("Got it")
            onClicked: help.close()
        }
    }
}
