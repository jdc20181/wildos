import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import WildOS.Settings
import WildOS.UI as UI

// Accessibility, laid out as SettingsAccessibility.dc.html: a line about the
// shortcuts, then Seeing on the left beside Hearing and Keyboard and touch
// on the right (one column when the window is narrow). Every switch is bound
// to Accessibility, which reads the system each time this page is made, so
// leaving and coming back always shows what is really set. Each row with a
// shortcut shows its keys; wildos-access makes them work on the lock and
// sign-in screens as well.
PageScroll {
    id: page

    readonly property bool wide: width >= 760

    function onOff(on) {
        return on ? "On" : "Off";
    }

    Component.onCompleted: {
        Accessibility.refresh();
        Accessibility.refreshMagnifier();
    }

    // KWin says nothing when zoom changes (Super and plus from the
    // keyboard), so the switch looks while the page is showing.
    Timer {
        interval: 1000
        repeat: true
        running: page.visible && Qt.application.state === Qt.ApplicationActive
        onTriggered: Accessibility.refreshMagnifier()
    }

    UI.Label {
        Layout.fillWidth: true
        Layout.topMargin: -6
        text: "Every shortcut here also works on the sign-in and unlock screens, so help is there before you've signed in."
        size: 13
        color: UI.Style.inkStatus
        wrapMode: Text.Wrap
        elide: Text.ElideNone
        lineHeight: 1.5
    }

    GridLayout {
        Layout.fillWidth: true
        columns: page.wide ? 2 : 1
        columnSpacing: 20
        rowSpacing: 18

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.horizontalStretchFactor: 108
            Layout.alignment: Qt.AlignTop
            spacing: 10

            SectionTitle {
                text: "Seeing"
            }

            UI.Card {
                Layout.fillWidth: true

                Column {
                    width: parent.width

                    UI.SettingRow {
                        title: "Text size"
                        description: "Larger text everywhere, without changing scale"

                        TextSizeBox {
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    UI.SettingRow {
                        id: pointerRow

                        readonly property var sizes: Accessibility.pointerSizes

                        function nearest(size) {
                            let best = 0;
                            for (let i = 1; i < sizes.length; ++i) {
                                if (Math.abs(sizes[i] - size) < Math.abs(sizes[best] - size)) {
                                    best = i;
                                }
                            }
                            return best;
                        }

                        title: "Pointer"
                        description: "Size and colour"

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 16

                            UI.Slider {
                                id: pointerSize
                                anchors.verticalCenter: parent.verticalCenter
                                implicitWidth: 90
                                implicitHeight: 20
                                flat: true
                                from: 0
                                to: Math.max(1, pointerRow.sizes.length - 1)
                                stepSize: 1
                                snapMode: T.Slider.SnapAlways
                                wheelEnabled: false
                                value: pointerRow.nearest(Accessibility.pointerSize)
                                accessibleName: "Pointer size"
                                Accessible.description: pointerRow.sizes[Math.round(value)] + " pixels"
                                // Applied once the slider settles, so dragging
                                // across it does not reload the pointer at
                                // every step.
                                onMoved: pointerApply.restart()

                                Timer {
                                    id: pointerApply
                                    interval: 250
                                    onTriggered: {
                                        if (pointerSize.pressed) {
                                            restart();
                                            return;
                                        }
                                        Accessibility.setPointerSize(pointerRow.sizes[Math.round(pointerSize.value)]);
                                        pointerSize.value = Qt.binding(() => pointerRow.nearest(Accessibility.pointerSize));
                                    }
                                }
                            }

                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 8
                                Accessible.role: Accessible.Grouping
                                Accessible.name: "Pointer colour"

                                Repeater {
                                    model: Accessibility.pointerColours

                                    UI.Swatch {
                                        required property var modelData
                                        colour: modelData.colour
                                        name: modelData.name
                                        selected: Accessibility.pointerColour === modelData.id
                                        onClicked: Accessibility.setPointerColour(modelData.id)
                                    }
                                }
                            }
                        }
                    }

                    UI.SettingRow {
                        title: "Magnifier"
                        description: "Zoom into any part of the screen"
                        status: page.onOff(Accessibility.magnifier)
                        below: UI.ShortcutKeys {
                            keys: ["Super", "+"]
                        }

                        UI.Toggle {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: Accessibility.magnifier
                            Accessible.name: "Magnifier"
                            onToggled: Accessibility.setMagnifier(checked)
                        }
                    }

                    UI.SettingRow {
                        title: "Screen reader"
                        description: "Reads out what's on screen"
                        status: page.onOff(Accessibility.screenReader)
                        below: UI.ShortcutKeys {
                            keys: ["Super", "Alt", "S"]
                        }

                        UI.Toggle {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: Accessibility.screenReader
                            locked: !Accessibility.screenReaderInstalled
                            lockedReason: "The screen reader isn't installed on this computer"
                            Accessible.name: "Screen reader"
                            onToggled: Accessibility.setScreenReader(checked)
                        }
                    }

                    UI.SettingRow {
                        title: "High contrast"
                        description: "Strong colours and clear edges"
                        status: page.onOff(Accessibility.highContrast)
                        below: UI.ShortcutKeys {
                            keys: ["Super", "Alt", "H"]
                        }

                        UI.Toggle {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: Accessibility.highContrast
                            enabled: !Accessibility.highContrastBusy
                            locked: !Accessibility.highContrastInstalled
                            lockedReason: "The high contrast theme isn't installed on this computer"
                            Accessible.name: "High contrast"
                            onToggled: Accessibility.setHighContrast(checked)
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.horizontalStretchFactor: 100
            Layout.alignment: Qt.AlignTop
            spacing: 18

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                SectionTitle {
                    text: "Hearing"
                }

                UI.Card {
                    Layout.fillWidth: true

                    Column {
                        width: parent.width

                        UI.SettingRow {
                            title: "Flash the screen for alert sounds"
                            description: "For when you can't hear them"
                            status: page.onOff(Accessibility.visibleBell)

                            UI.Toggle {
                                anchors.verticalCenter: parent.verticalCenter
                                checked: Accessibility.visibleBell
                                Accessible.name: "Visual alerts"
                                onToggled: Accessibility.setVisibleBell(checked)
                            }
                        }

                        UI.SettingRow {
                            title: "Mono audio"
                            description: "Both ears get the same sound"
                            status: page.onOff(Accessibility.monoAudio)

                            UI.Toggle {
                                anchors.verticalCenter: parent.verticalCenter
                                checked: Accessibility.monoAudio
                                locked: !Accessibility.monoAudioInstalled
                                lockedReason: "Mono audio isn't available on this computer"
                                Accessible.name: "Mono audio"
                                onToggled: Accessibility.setMonoAudio(checked)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                SectionTitle {
                    text: "Keyboard and touch"
                }

                UI.Card {
                    Layout.fillWidth: true

                    Column {
                        width: parent.width

                        UI.SettingRow {
                            title: "On-screen keyboard"
                            description: "Type with a mouse, touch or switch"
                            status: page.onOff(Accessibility.onScreenKeyboard)
                            below: UI.ShortcutKeys {
                                keys: ["Super", "Ctrl", "O"]
                            }

                            UI.Toggle {
                                anchors.verticalCenter: parent.verticalCenter
                                checked: Accessibility.onScreenKeyboard
                                locked: !Accessibility.onScreenKeyboardInstalled
                                lockedReason: "No on-screen keyboard is installed on this computer"
                                Accessible.name: "On-screen keyboard"
                                onToggled: Accessibility.setOnScreenKeyboard(checked)
                            }
                        }

                        UI.SettingRow {
                            title: "Sticky keys"
                            description: "Press shortcuts one at a time"
                            status: page.onOff(Accessibility.stickyKeys)
                            below: UI.ShortcutKeys {
                                keys: ["Shift ×5"]
                            }

                            UI.Toggle {
                                anchors.verticalCenter: parent.verticalCenter
                                checked: Accessibility.stickyKeys
                                Accessible.name: "Sticky keys"
                                onToggled: Accessibility.setStickyKeys(checked)
                            }
                        }

                        UI.SettingRow {
                            readonly property var info: Pages.page("slowbouncekeys")
                            title: info.title
                            description: info.description
                            chevron: true
                            onClicked: Navigator.open("slowbouncekeys")
                        }
                    }
                }
            }
        }
    }
}
