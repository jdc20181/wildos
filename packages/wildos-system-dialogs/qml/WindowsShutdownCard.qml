import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import WildOS.UI as UI

// "Your Windows apps are still open" (WinModeShutdown.dc, board 9.9),
// asked by Windows mode's shutdown guard as the session ends: 640 wide,
// radius 18, a 20px title, the open documents with their state, the
// accent card saying what keeping does, and a footer with "Close without
// saving" at the left. After 60 seconds the safe choice is taken.
//
// The taskbar panel's Restart and Shut down for Windows alone ask the same
// way (actions windows-restart and windows-shutdown) when an app may have
// unsaved work, with no countdown: restarting can't keep the work, so Go
// back is the main button; shutting down offers Save to disk instead,
// which keeps it and still frees the memory.
Item {
    id: card

    // shutdown, restart or signout; windows-restart or windows-shutdown
    property string action: "shutdown"
    property var apps: []
    property string reopen
    property int seconds: 0

    signal keep()
    signal discard()
    signal back()

    readonly property bool windowsOnly: action.startsWith("windows-")
    readonly property bool canKeep: action !== "windows-restart"
    readonly property string keepText: action === "windows-shutdown" ? "Save to disk instead"
        : action === "restart" ? "Keep my work and restart" : action === "signout" ? "Keep my work and sign out" : "Keep my work and shut down"
    readonly property string keepDetail: action === "windows-shutdown"
        ? "Windows mode is saved to disk as it is, in about 5 seconds, and uses no memory until you open a Windows app."
        : "Windows mode is saved to disk as it is, in about 5 seconds. Next time, " + (card.reopen.startsWith("Your ") ? "your " + card.reopen.slice(5) : card.reopen)
    readonly property string discardText: action === "windows-restart" ? "Restart without saving"
        : action === "windows-shutdown" ? "Shut down without saving" : "Close without saving"
    readonly property string body: action === "windows-restart"
        ? "Restarting Windows closes them, and changes that aren't saved are lost. Save your work in them first, then restart."
        : action === "windows-shutdown"
        ? "Shutting down Windows closes them, and changes that aren't saved are lost. Save your work in them first, or save Windows mode to disk instead, which keeps everything as it is."
        : "Nothing will be lost. Wild OS can keep them exactly as they are, unsaved changes included, and reopen them next time you "
            + (action === "signout" ? "sign in." : "switch on.")
    readonly property string title: "Your Windows apps are still open"

    width: 640
    implicitHeight: column.implicitHeight
    Accessible.role: Accessible.AlertMessage
    Accessible.name: title

    Rectangle {
        anchors.fill: parent
        radius: UI.Style.radiusDialog
        color: UI.Style.glassDialog
        border.width: 1
        border.color: UI.Style.borderGlass
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: UI.Style.shadowFlyout
            shadowVerticalOffset: 30
            shadowBlur: 1.0
            blurMax: 64
        }
    }

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 28
            Layout.bottomMargin: 20
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                UI.Label {
                    Layout.fillWidth: true
                    text: card.title
                    size: 20
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                }

                UI.Label {
                    Layout.fillWidth: true
                    text: card.body
                    size: 13
                    color: UI.Style.inkAlpha(0.82, 0.76)
                    wrapMode: Text.Wrap
                    elide: Text.ElideNone
                    lineHeight: 1.55
                }
            }

            UI.Card {
                Layout.fillWidth: true

                Column {
                    width: parent.width

                    Repeater {
                        model: card.apps

                        UI.ListRow {
                            required property var modelData
                            minHeight: 58
                            title: modelData.document
                            description: modelData.app
                            leading: UI.ThemeIcon {
                                width: 30
                                height: 30
                                name: modelData.icon
                                fallback: "wildos-windows-app"
                            }

                            UI.Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.state === "unsaved" ? "Unsaved changes" : modelData.state === "saved" ? "Saved" : "Open"
                                size: 12
                                color: modelData.state === "unsaved" ? UI.Style.warning : UI.Style.inkMuted
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: card.canKeep
                Layout.fillWidth: true
                implicitHeight: keepRow.implicitHeight + 32
                radius: UI.Style.radiusMd
                color: UI.Style.accentTint
                border.width: 1
                border.color: UI.Style.accentBorder

                RowLayout {
                    id: keepRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 16

                    Rectangle {
                        Layout.alignment: Qt.AlignTop
                        implicitWidth: 44
                        implicitHeight: 44
                        radius: UI.Style.radiusMd
                        color: UI.Style.surfaceControl

                        UI.Glyph {
                            anchors.centerIn: parent
                            width: 22
                            height: 22
                            // The design's floppy disk: what "keep" means.
                            path: "M5 3h11l3 3v15H5zM8 3v5h7V3M8 21v-7h8v7"
                            strokeWidth: 1.9
                            color: UI.Style.accentText
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        UI.Label {
                            text: card.keepText
                            size: 15
                            font.weight: Font.DemiBold
                        }

                        UI.Label {
                            Layout.fillWidth: true
                            text: card.keepDetail
                            size: 13
                            color: UI.Style.inkStatus
                            wrapMode: Text.Wrap
                            elide: Text.ElideNone
                            lineHeight: 1.45
                        }
                    }
                }
            }

            UI.Label {
                Layout.fillWidth: true
                visible: card.seconds > 0
                text: "If you don't choose, Wild OS keeps your work in " + card.seconds + (card.seconds === 1 ? " second." : " seconds.")
                size: 12
                color: UI.Style.inkMuted
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 64
            bottomLeftRadius: UI.Style.radiusDialog
            bottomRightRadius: UI.Style.radiusDialog
            color: UI.Style.shade(0.25, 0.04)

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: UI.Style.borderSubtle
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                spacing: 10

                UI.LinkButton {
                    id: discardLink
                    text: card.discardText
                    onClicked: card.discard()
                }

                Item {
                    Layout.fillWidth: true
                }

                UI.Button {
                    id: backButton
                    kind: card.canKeep ? "secondary" : "primary"
                    text: "Go back"
                    onClicked: card.back()
                }

                UI.Button {
                    id: keepButton
                    visible: card.canKeep
                    kind: "primary"
                    text: card.keepText
                    onClicked: card.keep()
                }
            }
        }
    }

    Keys.onEscapePressed: card.back()
    // Enter presses the button that has focus, which is the safe one to
    // start with; the buttons themselves only take Space.
    function enter() {
        if (discardLink.activeFocus) {
            card.discard();
        } else if (keepButton.activeFocus && card.canKeep) {
            card.keep();
        } else {
            card.back();
        }
    }
    Keys.onReturnPressed: card.enter()
    Keys.onEnterPressed: card.enter()
    function takeFocus() {
        (card.canKeep ? keepButton : backButton).forceActiveFocus();
    }
}
