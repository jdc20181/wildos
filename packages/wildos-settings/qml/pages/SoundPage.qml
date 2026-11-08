import QtQuick
import QtQuick.Layouts
import org.kde.plasma.private.volume
import WildOS.Settings
import WildOS.UI as UI

// Sound, after Windows 11: where sound plays and how loud, then the
// microphone. Devices and volumes come from plasma-pa, the same service the
// taskbar's volume uses, so both always agree.
PageScroll {
    id: page

    function nodeName(device) {
        return device ? (device.description || device.name || "Unknown device") : "";
    }

    function percent(volume) {
        return Math.round(volume / PulseAudio.NormalVolume * 100);
    }

    function options(list) {
        return list.map(d => ({ value: d.index, text: page.nodeName(d) }));
    }

    function byIndex(list, index) {
        return list.find(d => d.index === index) || null;
    }

    // Physical devices only, as the volume flyout lists them.
    PulseObjectFilterModel {
        id: sinks
        filterOutInactiveDevices: true
        filterVirtualDevices: true
        sourceModel: SinkModel {}
    }

    PulseObjectFilterModel {
        id: sources
        filterOutInactiveDevices: true
        filterVirtualDevices: true
        sourceModel: SourceModel {}
    }

    property var outputs: []
    property var inputs: []

    Instantiator {
        model: sinks
        delegate: QtObject {
            required property var model
            readonly property var device: model.PulseObject
        }
        onObjectAdded: page.outputs = page.collect(this)
        onObjectRemoved: page.outputs = page.collect(this)
    }

    Instantiator {
        model: sources
        delegate: QtObject {
            required property var model
            readonly property var device: model.PulseObject
        }
        onObjectAdded: page.inputs = page.collect(this)
        onObjectRemoved: page.inputs = page.collect(this)
    }

    function collect(instantiator) {
        const list = [];
        for (let i = 0; i < instantiator.count; ++i) {
            const o = instantiator.objectAt(i);
            if (o && o.device) {
                list.push(o.device);
            }
        }
        return list;
    }

    readonly property var output: PreferredDevice.sink && PreferredDevice.sink.name !== "auto_null" ? PreferredDevice.sink : null
    readonly property var input: PreferredDevice.source

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Output"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "sound"
                    title: "Choose where to play sound"
                    description: page.output ? "Apps may have their own settings" : "No speakers or headphones found"

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: page.outputs.length > 0
                        Accessible.name: "Output device"
                        model: page.options(page.outputs)
                        readonly property var choice: page.output ? page.output.index : -1
                        currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                        onActivated: index => {
                            const value = model[index].value;
                            const device = page.byIndex(page.outputs, value);
                            if (device) {
                                device.default = true;
                            }
                        }
                    }
                }

                VolumeRow {
                    visible: page.output !== null
                    title: "Volume"
                    device: page.output
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Input"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "mic"
                    title: "Choose a device for speaking or recording"
                    description: page.input ? "Apps may have their own settings" : "No microphone found"

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: page.inputs.length > 0
                        Accessible.name: "Input device"
                        model: page.options(page.inputs)
                        readonly property var choice: page.input ? page.input.index : -1
                        currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                        onActivated: index => {
                            const value = model[index].value;
                            const device = page.byIndex(page.inputs, value);
                            if (device) {
                                device.default = true;
                            }
                        }
                    }
                }

                VolumeRow {
                    visible: page.input !== null
                    title: "Volume"
                    device: page.input
                }
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                visible: Pages.exists("soundadvanced")
                title: Pages.page("soundadvanced").title
                description: Pages.page("soundadvanced").description
                chevron: true
                onClicked: Navigator.open("soundadvanced")
            }
        }
    }

    component VolumeRow: UI.SettingRow {
        id: row

        property var device: null
        readonly property bool muted: device !== null && device.muted

        status: device ? (muted ? "Muted" : page.percent(device.volume) + "%") : ""

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            UI.IconButton {
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                height: 34
                iconName: row.muted ? "sound-off" : "sound"
                iconSize: 18
                tip: row.muted ? "Unmute" : "Mute"
                onClicked: row.device.muted = !row.device.muted
            }

            UI.Slider {
                anchors.verticalCenter: parent.verticalCenter
                width: 240
                accessibleName: row.title
                dimmed: row.muted
                to: 100
                value: row.device ? Math.min(100, page.percent(row.device.volume)) : 0
                onMoved: {
                    row.device.volume = Math.round(value / 100 * PulseAudio.NormalVolume);
                    if (row.device.muted && value > 0) {
                        row.device.muted = false;
                    }
                }
            }
        }
    }
}
