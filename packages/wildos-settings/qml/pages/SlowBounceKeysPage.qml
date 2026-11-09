import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Accessibility > Slow and bounce keys: kaccessrc [Keyboard] SlowKeys and
// BounceKeys with their delays, as kcm_access keeps them. KWin applies
// bounce keys itself; slow keys come from the Wild OS KWin plugin
// (wildos-shell), as KWin 6.3 has none of its own.
PageScroll {
    id: page

    function seconds(ms) {
        const s = Math.round(ms / 100) / 10;
        return s === 1 ? "1 second" : s + " seconds";
    }

    Component.onCompleted: Accessibility.refresh()

    component DelaySlider: UI.Slider {
        id: slider

        property int delay
        signal settled(int ms)

        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        implicitWidth: 180
        from: 100
        to: 2000
        stepSize: 100
        value: delay
        // Written once the slider settles rather than at every step.
        onMoved: settle.restart()

        Timer {
            id: settle
            interval: 300
            onTriggered: {
                if (slider.pressed) {
                    restart();
                    return;
                }
                slider.settled(Math.round(slider.value));
                slider.value = Qt.binding(() => slider.delay);
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Slow keys"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    title: "Slow keys"
                    description: "Only count a key that's held down for a moment, so brushing one does nothing"
                    status: Accessibility.slowKeys ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: Accessibility.slowKeys
                        Accessible.name: "Slow keys"
                        onToggled: Accessibility.setSlowKeys(checked)
                    }
                }

                UI.SettingRow {
                    title: "How long to hold a key"
                    description: page.seconds(slowDelay.value)
                    enabled: Accessibility.slowKeys

                    DelaySlider {
                        id: slowDelay
                        delay: Accessibility.slowKeysDelay
                        accessibleName: "How long to hold a key"
                        onSettled: ms => Accessibility.setSlowKeysDelay(ms)
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Bounce keys"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    title: "Bounce keys"
                    description: "Ignore a key pressed again straight away, so a shaky press types one letter"
                    status: Accessibility.bounceKeys ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: Accessibility.bounceKeys
                        Accessible.name: "Bounce keys"
                        onToggled: Accessibility.setBounceKeys(checked)
                    }
                }

                UI.SettingRow {
                    title: "Ignore repeats within"
                    description: page.seconds(bounceDelay.value)
                    enabled: Accessibility.bounceKeys

                    DelaySlider {
                        id: bounceDelay
                        delay: Accessibility.bounceKeysDelay
                        accessibleName: "Ignore repeats within"
                        onSettled: ms => Accessibility.setBounceKeysDelay(ms)
                    }
                }
            }
        }
    }
}
