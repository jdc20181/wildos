pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import WildOS.Terminal
import WildOS.UI as UI

// Appearance: colour scheme, font, cursor and transparency. Every change
// shows in the open terminals straight away.
PageScroll {
    id: page

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Colour scheme"
        }

        UI.Label {
            Layout.fillWidth: true
            text: "Homebrew is green text on black, like the Homebrew theme of the Terminal on a Mac."
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }

        Flow {
            Layout.fillWidth: true
            spacing: 12

            Repeater {
                model: Config.schemes

                SchemeCard {
                    required property var modelData
                    scheme: modelData
                    checked: Config.scheme === modelData.id
                    onClicked: Config.scheme = modelData.id
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Text"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "font"
                    title: "Font"
                    description: "Fonts with letters all the same width."

                    Choice {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "Font"
                        options: Config.monospaceFonts.map(f => ({ value: f, text: f }))
                        value: Config.fontFamily
                        onChosen: v => Config.fontFamily = v
                    }
                }

                UI.SettingRow {
                    iconName: "font"
                    title: "Font size"
                    description: "Ctrl+plus and Ctrl+minus zoom one tab for a while; this is the size every tab starts at."

                    Choice {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "Font size"
                        options: [8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 24, 28].map(n => ({ value: n, text: n + " pt" }))
                        value: Config.fontSize
                        onChosen: v => Config.fontSize = v
                    }
                }

                ToggleRow {
                    iconName: "bolt"
                    title: "Show bold text in bright colours"
                    description: "Bold green turns bright green, as programs written for the classic terminals expect."
                    value: Config.boldBright
                    onSwitched: v => Config.boldBright = v
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Cursor"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "pencil"
                    title: "Cursor shape"

                    Choice {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "Cursor shape"
                        options: [
                            { value: "bar", text: "Bar" },
                            { value: "block", text: "Block" },
                            { value: "underline", text: "Underline" }
                        ]
                        value: Config.cursorShape
                        onChosen: v => Config.cursorShape = v
                    }
                }

                ToggleRow {
                    iconName: "clock"
                    title: "Blinking cursor"
                    value: Config.cursorBlink
                    onSwitched: v => Config.cursorBlink = v
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Transparency"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "drop"
                    title: "Background opacity"
                    description: "How much of the desktop shows through behind the text."

                    UI.Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: UI.Style.px(40)
                        horizontalAlignment: Text.AlignRight
                        text: slider.value + "%"
                        size: 12
                        color: UI.Style.inkMuted
                    }

                    UI.Slider {
                        id: slider
                        anchors.verticalCenter: parent.verticalCenter
                        width: 220
                        from: 30
                        to: 100
                        stepSize: 5
                        value: Config.opacity
                        accessibleName: "Background opacity"
                        onMoved: Config.opacity = value
                    }
                }

                ToggleRow {
                    iconName: "drop"
                    title: "Frosted glass"
                    description: !host.blurAvailable ? "Turn on Transparency effects in Settings, Personalisation to use this."
                               : Config.opacity >= 100 ? "Blurs the desktop behind the text once the background is see-through."
                               : "Blurs the desktop behind the text so it stays easy to read."
                    enabled: host.blurAvailable && Config.opacity < 100
                    value: Config.blur
                    onSwitched: v => Config.blur = v
                }
            }
        }
    }
}
