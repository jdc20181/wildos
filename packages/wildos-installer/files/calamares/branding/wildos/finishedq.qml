/* Finished (InstallerDone.dc). Restart now, the only button, is in the
 * footer (calamares-navigation.qml); it waits until the recovery key is
 * saved to a USB stick or written down, because the key exists nowhere else:
 * it is never written to the installed disk. If the install failed, the
 * page shows where it stopped instead.
 */
import io.calamares.core 1.0
import io.calamares.ui 1.0
import WildOS.Installer
import WildOS.UI as UI

import QtQuick
import QtQuick.Layouts

Item {
    id: page

    readonly property bool failed: config.failed
    property bool keptOther: false
    property bool keyAdded: false
    property bool keyAsked: false
    property bool wroteDown: false
    property bool choosingDrive: false
    property string saveProblem: ""
    readonly property bool keyPending: keyAdded && !Installer.keySaved

    // A failed install must never restart on its own when the window closes.
    function holdRestartIfFailed() {
        if (failed)
            config.restartNowWanted = false;
    }
    onFailedChanged: holdRestartIfFailed()
    Component.onCompleted: {
        Installer.finished = config;
        holdRestartIfFailed();
    }

    function onActivate() {
        var plan = Global.value("wildosPlan") || {};
        keptOther = plan.mode !== "erase" && (plan.other || "") !== "";
        keyAsked = !!Global.value("wildosRecovery");
        keyAdded = keyAsked && !!Global.value("wildosRecoveryAdded");
        holdRestartIfFailed();
    }

    ColumnLayout {
        visible: !page.failed
        anchors.centerIn: parent
        width: Math.min(parent.width, 600)
        spacing: 22

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 96
            Layout.preferredHeight: 96
            source: "mark-large.png"
            sourceSize: Qt.size(192, 192)
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Wild OS is installed")
            color: UI.Style.ink
            font.family: UI.Style.fontFamily
            font.pixelSize: 30
            font.weight: UI.Style.weightSemiBold
            font.letterSpacing: -0.3
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 460
            horizontalAlignment: Text.AlignHCenter
            text: page.keptOther
                  ? qsTr("Remove the USB stick, then restart. Next you'll create your account, join Wi-Fi and bring your files across.")
                  : qsTr("Remove the USB stick, then restart. Next you'll create your account and join Wi-Fi.")
            color: Qt.rgba(232 / 255, 236 / 255, 248 / 255, 0.82)
            font.family: UI.Style.fontFamily
            font.letterSpacing: UI.Style.tracking(15)
            font.pixelSize: 15
            lineHeight: 1.55
            wrapMode: Text.WordWrap
        }

        // The recovery key, until it is saved or written down.
        Rectangle {
            visible: page.keyPending
            Layout.fillWidth: true
            implicitHeight: keyCard.implicitHeight + 36
            radius: UI.Style.radiusMd
            color: UI.Style.accentTint
            border.width: 1
            border.color: UI.Style.accentBorder

            ColumnLayout {
                id: keyCard
                anchors.fill: parent
                anchors.margins: 18
                spacing: 10

                Text {
                    text: qsTr("Your recovery key")
                    color: UI.Style.ink
                    font.family: UI.Style.fontFamily
                    font.letterSpacing: UI.Style.tracking(14)
                    font.pixelSize: 14
                    font.weight: UI.Style.weightSemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: Installer.recoveryKey
                    color: UI.Style.inkStrong
                    font.family: "monospace"
                    font.pixelSize: 19
                    font.letterSpacing: 1
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("It unlocks the disk if you forget your disk password. This is the only time it is shown, and it is not kept on this computer.")
                    color: UI.Style.inkStatus
                    font.family: UI.Style.fontFamily
                    font.letterSpacing: UI.Style.tracking(12)
                    font.pixelSize: 12
                    lineHeight: 1.4
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    visible: !page.choosingDrive
                    spacing: 10
                    UI.Button {
                        kind: "primary"
                        text: qsTr("Save to a USB stick")
                        onClicked: {
                            page.saveProblem = "";
                            Installer.refreshDrives();
                            page.choosingDrive = true;
                        }
                    }
                    UI.Button {
                        text: qsTr("I've written it down")
                        onClicked: {
                            page.wroteDown = true;
                            Installer.keySaved = true;
                        }
                    }
                }
                ColumnLayout {
                    visible: page.choosingDrive
                    Layout.fillWidth: true
                    spacing: 6
                    Text {
                        Layout.fillWidth: true
                        text: Installer.drives.length === 0
                              ? qsTr("Plug in a USB stick, then press Look again.")
                              : qsTr("Save it on:")
                        color: UI.Style.inkStatus
                        font.family: UI.Style.fontFamily
                        font.letterSpacing: UI.Style.tracking(12)
                        font.pixelSize: 12
                    }
                    Repeater {
                        model: Installer.drives
                        delegate: UI.Button {
                            required property var modelData
                            text: qsTr("%1 (%2)").arg(modelData.name).arg(modelData.size)
                            onClicked: {
                                page.saveProblem = Installer.saveKey(modelData.device);
                                if (page.saveProblem === "")
                                    page.choosingDrive = false;
                            }
                        }
                    }
                    RowLayout {
                        spacing: 10
                        UI.Button {
                            text: qsTr("Look again")
                            onClicked: Installer.refreshDrives()
                        }
                        UI.Button {
                            kind: "text"
                            text: qsTr("Cancel")
                            onClicked: page.choosingDrive = false
                        }
                    }
                    Text {
                        visible: page.saveProblem !== ""
                        text: page.saveProblem
                        color: UI.Style.danger
                        font.family: UI.Style.fontFamily
                        font.letterSpacing: UI.Style.tracking(12)
                        font.pixelSize: 12
                    }
                }
            }
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(page.width, infoText.implicitWidth + 40)
            visible: infoText.lines.length > 0
            implicitHeight: infoText.implicitHeight + 32
            radius: UI.Style.radiusMd
            color: UI.Style.surfaceRaised
            border.width: 1
            border.color: UI.Style.borderSubtle

            ColumnLayout {
                id: infoText
                readonly property var lines: {
                    var out = [];
                    if (page.keptOther)
                        out.push(qsTr("When the computer starts you'll see a menu to choose Wild OS or Windows."));
                    if (page.keyAdded && Installer.keySaved)
                        out.push(page.wroteDown
                                 ? qsTr("You wrote down your recovery key. Keep it somewhere other than this computer.")
                                 : qsTr("Your recovery key was saved. Keep it somewhere other than this computer."));
                    if (page.keyAsked && !page.keyAdded)
                        out.push(qsTr("The recovery key could not be added, so only your disk password unlocks the disk."));
                    return out;
                }
                anchors.fill: parent
                anchors.margins: 16
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 8
                Repeater {
                    model: infoText.lines
                    delegate: Text {
                        required property string modelData
                        text: modelData
                        color: Qt.rgba(232 / 255, 236 / 255, 248 / 255, 0.85)
                        font.family: UI.Style.fontFamily
                        font.letterSpacing: UI.Style.tracking(13)
                        font.pixelSize: 13
                    }
                }
            }
        }
    }

    WPage {
        visible: page.failed
        anchors.fill: parent
        title: qsTr("The install did not finish")
        subtitle: qsTr("Something went wrong partway through. The details below show where it stopped.")

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 14
            color: Qt.rgba(242 / 255, 160 / 255, 138 / 255, 0.08)
            border.width: 1
            border.color: Qt.rgba(242 / 255, 160 / 255, 138 / 255, 0.30)
            clip: true

            Flickable {
                id: details
                anchors.fill: parent
                anchors.margins: 18
                contentHeight: failure.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: failure
                    width: details.width
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: config.failureMessage
                        wrapMode: Text.WordWrap
                        color: UI.Style.ink
                        font.family: UI.Style.fontFamily
                        font.letterSpacing: UI.Style.tracking(14)
                        font.pixelSize: 14
                        font.weight: UI.Style.weightSemiBold
                    }
                    Text {
                        Layout.fillWidth: true
                        // Calamares marks up the details with HTML, such as the failed command in italics.
                        text: config.failureDetails
                        textFormat: Text.StyledText
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        color: UI.Style.inkStatus
                        font.family: UI.Style.fontFamily
                        font.letterSpacing: UI.Style.tracking(12)
                        font.pixelSize: 12
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("A full log is saved as installation.log in the live session's Home folder.")
                        wrapMode: Text.WordWrap
                        color: UI.Style.inkMuted
                        font.family: UI.Style.fontFamily
                        font.letterSpacing: UI.Style.tracking(12)
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
