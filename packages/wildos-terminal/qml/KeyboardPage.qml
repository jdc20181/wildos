pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import WildOS.Terminal
import WildOS.UI as UI

// Keyboard shortcuts: every key Terminal itself answers to, the same as
// Windows Terminal's where it has one.
PageScroll {
    id: page

    readonly property var groups: [
        { title: "Tabs", keys: [
            ["New tab", "Ctrl+Shift+T"],
            ["New tab with profile 1 to 9", "Ctrl+Shift+1 to 9"],
            ["Duplicate tab", "Ctrl+Shift+D"],
            ["Close tab or pane", "Ctrl+Shift+W"],
            ["Next tab", "Ctrl+Tab"],
            ["Previous tab", "Ctrl+Shift+Tab"],
            ["Go to tab 1 to 8, or the last tab", "Ctrl+Alt+1 to 9"],
            ["Open the new tab menu", "Ctrl+Shift+Space"]
        ] },
        { title: "Panes", keys: [
            ["Split right", "Alt+Shift+="],
            ["Split down", "Alt+Shift+-"],
            ["Move to the pane beside", "Alt+Arrows"]
        ] },
        { title: "Text", keys: [
            ["Copy", Config.ctrlCopyPaste ? "Ctrl+Shift+C or Ctrl+C" : "Ctrl+Shift+C"],
            ["Paste", Config.ctrlCopyPaste ? "Ctrl+Shift+V or Ctrl+V" : "Ctrl+Shift+V"],
            ["Make text bigger", "Ctrl+="],
            ["Make text smaller", "Ctrl+-"],
            ["Reset text size", "Ctrl+0"],
            ["Open a link", "Ctrl+click"]
        ] },
        { title: "Window", keys: [
            ["New window", "Ctrl+Shift+N"],
            ["Settings", "Ctrl+,"],
            ["Full screen", "F11"]
        ] }
    ]

    Repeater {
        model: page.groups

        ColumnLayout {
            id: group
            required property var modelData
            Layout.fillWidth: true
            spacing: 10

            SectionTitle {
                text: group.modelData.title
            }

            UI.Card {
                Layout.fillWidth: true

                Column {
                    width: parent.width

                    Repeater {
                        model: group.modelData.keys

                        UI.SettingRow {
                            id: row
                            required property var modelData
                            title: modelData[0]

                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6

                                Repeater {
                                    model: row.modelData[1].split(" ")

                                    Item {
                                        id: word
                                        required property string modelData
                                        readonly property bool joiner: modelData === "or" || modelData === "to"
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: joiner ? joinText.implicitWidth : keys.implicitWidth
                                        height: Math.max(26, UI.Style.px(12) + 10)

                                        UI.Label {
                                            id: joinText
                                            visible: word.joiner
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: word.modelData
                                            size: 12
                                            color: UI.Style.inkMuted
                                        }

                                        Row {
                                            id: keys
                                            visible: !word.joiner
                                            anchors.verticalCenter: parent.verticalCenter
                                            spacing: 4

                                            Repeater {
                                                model: word.modelData.split("+")

                                                Rectangle {
                                                    id: cap
                                                    required property string modelData
                                                    width: Math.max(height, capText.implicitWidth + 16)
                                                    height: Math.max(26, UI.Style.px(12) + 10)
                                                    radius: UI.Style.radiusXs
                                                    color: UI.Style.tone(0.07)
                                                    border.width: 1
                                                    border.color: UI.Style.borderMenu

                                                    UI.Label {
                                                        id: capText
                                                        anchors.centerIn: parent
                                                        text: cap.modelData
                                                        size: 12
                                                        font.weight: Font.Medium
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
