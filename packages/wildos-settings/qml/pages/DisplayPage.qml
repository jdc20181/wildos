import QtQuick
import QtQuick.Layouts
import org.kde.plasma.private.brightnesscontrolplugin
import WildOS.Settings
import WildOS.UI as UI

// Display, after Windows 11: brightness and night light, then scale,
// resolution and orientation for each screen. Brightness goes through
// powerdevil like the quick settings slider; screens through KScreen, so a
// change is kept for the next sign-in.
PageScroll {
    id: page

    Component.onCompleted: System.refreshDisplays()

    ScreenBrightnessControl {
        id: brightness
        isSilent: true
    }

    NightLightControl {
        id: nightLight
    }

    property var dimmable: []

    Instantiator {
        id: dimmableDisplays
        model: brightness.displays
        delegate: QtObject {
            required property string displayName
            required property string label
            required property int brightness
            required property int maxBrightness
        }
        onObjectAdded: page.dimmable = page.collect()
        onObjectRemoved: page.dimmable = page.collect()
    }

    function collect() {
        const list = [];
        for (let i = 0; i < dimmableDisplays.count; ++i) {
            list.push(dimmableDisplays.objectAt(i));
        }
        return list;
    }

    // The mode to go back to when a new resolution is not kept.
    property var pending: null

    function changeMode(display, modeId) {
        pending = { output: display.name, mode: display.currentMode };
        System.setDisplay(display.name, "mode", modeId);
        keepDialog.seconds = 15;
        keepDialog.open();
    }

    readonly property var rotations: [
        { value: 1, text: "Landscape", arg: "normal" },
        { value: 2, text: "Portrait", arg: "left" },
        { value: 4, text: "Landscape (flipped)", arg: "inverted" },
        { value: 8, text: "Portrait (flipped)", arg: "right" }
    ]

    function scaleOptions(current) {
        const list = [1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 3];
        if (!list.some(s => Math.abs(s - current) < 0.001)) {
            list.push(current);
            list.sort((a, b) => a - b);
        }
        return list.map(s => ({ value: s, text: Math.round(s * 100) + "%" }));
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            Repeater {
                model: page.dimmable

                UI.SettingRow {
                    id: brightnessRow
                    required property var modelData
                    readonly property int percent: modelData.maxBrightness > 0 ? Math.round(modelData.brightness / modelData.maxBrightness * 100) : 0
                    iconName: "sun"
                    title: page.dimmable.length > 1 ? "Brightness of " + modelData.label : "Brightness"
                    description: page.dimmable.length > 1 ? "Makes this screen brighter or dimmer" : "Makes the screen brighter or dimmer"
                    status: percent + "%"

                    UI.Slider {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 240
                        accessibleName: brightnessRow.title
                        from: 1
                        value: brightnessRow.percent
                        onMoved: brightness.setBrightness(brightnessRow.modelData.displayName,
                            Math.max(1, Math.round(value / 100 * brightnessRow.modelData.maxBrightness)))
                    }
                }
            }

            UI.SettingRow {
                visible: Pages.exists("nightlight")
                iconName: "moon"
                title: "Night light"
                description: "Warmer colours to help you sleep"
                status: nightLight.enabled ? (nightLight.inhibited ? "Paused" : "On") : "Off"
                chevron: true
                onClicked: Navigator.open("nightlight")
            }
        }
    }

    Repeater {
        model: System.displays

        ColumnLayout {
            id: screen
            required property var modelData
            required property int index
            Layout.fillWidth: true
            spacing: 10

            SectionTitle {
                text: System.displays.length > 1
                    ? "Screen " + (screen.index + 1) + (screen.modelData.builtIn ? ", built in" : ", " + screen.modelData.name)
                    : "Scale and layout"
            }

            UI.Card {
                Layout.fillWidth: true

                Column {
                    width: parent.width

                    UI.SettingRow {
                        iconName: "display"
                        title: "Scale"
                        description: "Makes text, apps and other items bigger"

                        UI.ComboBox {
                            anchors.verticalCenter: parent.verticalCenter
                            Accessible.name: "Scale"
                            model: page.scaleOptions(screen.modelData.scale)
                            readonly property var choice: screen.modelData.scale
                            currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                            onActivated: index => System.setDisplay(screen.modelData.name, "scale", String(model[index].value))
                        }
                    }

                    UI.SettingRow {
                        iconName: "resolution"
                        title: "Display resolution"
                        description: screen.modelData.currentRate > 0 ? "Refreshes " + Math.round(screen.modelData.currentRate) + " times a second" : ""

                        UI.ComboBox {
                            anchors.verticalCenter: parent.verticalCenter
                            Accessible.name: "Display resolution"
                            model: screen.modelData.modes.map((m, i) => ({ value: m.id, text: m.text + (i === 0 ? " (recommended)" : "") }))
                            readonly property var choice: {
                                const current = screen.modelData.modes.find(m => m.text === screen.modelData.currentSize);
                                return current ? current.id : "";
                            }
                            currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                            onActivated: index => page.changeMode(screen.modelData, model[index].value)
                        }
                    }

                    UI.SettingRow {
                        iconName: "rotate"
                        title: "Display orientation"

                        UI.ComboBox {
                            anchors.verticalCenter: parent.verticalCenter
                            Accessible.name: "Display orientation"
                            model: page.rotations
                            readonly property var choice: screen.modelData.rotation
                            currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                            onActivated: index => System.setDisplay(screen.modelData.name, "rotation", page.rotations.find(r => r.value === model[index].value).arg)
                        }
                    }
                }
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true
        visible: System.displays.length === 0

        Column {
            width: parent.width

            UI.SettingRow {
                iconName: "display"
                title: "Screen settings are not available"
                description: "Wild OS could not read the screens from the display service. Advanced display may still help."
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                visible: Pages.exists("displayadvanced")
                title: Pages.page("displayadvanced").title
                description: Pages.page("displayadvanced").description
                chevron: true
                onClicked: Navigator.open("displayadvanced")
            }
        }
    }

    UI.Dialog {
        id: keepDialog

        property int seconds: 15

        title: "Keep these display settings?"
        text: "Going back to the previous resolution in " + seconds + (seconds === 1 ? " second." : " seconds.")
        onOpened: countdown.restart()
        onClosed: countdown.stop()

        Timer {
            id: countdown
            interval: 1000
            repeat: true
            onTriggered: {
                keepDialog.seconds -= 1;
                if (keepDialog.seconds <= 0) {
                    keepDialog.revert();
                }
            }
        }

        function revert() {
            if (page.pending) {
                System.setDisplay(page.pending.output, "mode", page.pending.mode);
                page.pending = null;
            }
            close();
        }

        buttons: [
            UI.Button {
                text: "Keep changes"
                kind: "primary"
                onClicked: {
                    page.pending = null;
                    keepDialog.close();
                }
            },
            UI.Button {
                text: "Revert"
                onClicked: keepDialog.revert()
            }
        ]
    }
}
