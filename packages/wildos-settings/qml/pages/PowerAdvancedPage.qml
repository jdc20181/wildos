import QtQuick
import QtQuick.Layouts
import org.kde.plasma.private.battery
import WildOS.Settings
import WildOS.UI as UI

// More power settings, after Windows' "Lid and power button controls": what
// closing the lid and pressing the power button do, when the screen dims,
// and what happens when the battery is nearly flat. These are powerdevil's
// own settings in powerdevilrc, under the keys its settings module writes,
// and powerdevil rereads them when told to. Turning the screen off and
// sleeping are on Power and battery, so they are not repeated here.
PageScroll {
    id: page

    Component.onCompleted: PowerOptions.refresh()

    BatteryControlModel {
        id: battery
    }

    readonly property bool hasBattery: battery.hasInternalBatteries && battery.hasCumulative

    property int revision: 0

    // powerdevil's PowerButtonAction values (powerdevilenums.h).
    readonly property int doNothing: 0
    readonly property int sleep: 1
    readonly property int hibernate: 2
    readonly property int shutDown: 8
    readonly property int askWhatToDo: 16
    readonly property int lock: 32
    readonly property int screenOff: 64
    readonly property int screenToggle: 128

    readonly property var profiles: hasBattery
        ? [{ id: "Battery", prefix: "On battery power, " }, { id: "AC", prefix: "When plugged in, " }]
        : [{ id: "AC", prefix: "" }]

    function capitalise(text) {
        return text.charAt(0).toUpperCase() + text.slice(1);
    }

    function actionText(value) {
        switch (value) {
        case page.doNothing: return "Do nothing";
        case page.sleep: return "Sleep";
        case page.hibernate: return "Hibernate";
        case page.shutDown: return "Shut down";
        case page.askWhatToDo: return "Ask me what to do";
        case page.lock: return "Lock";
        case page.screenOff: return "Turn off the screen";
        case page.screenToggle: return "Turn the screen off or on";
        }
        return "Something else";
    }

    // The same choices powerdevil's own settings offer for each, without
    // sleep or hibernate where this computer cannot do them. A value set
    // elsewhere that is not on the list still shows, so nothing is changed
    // just by opening the page.
    function actionChoices(values, current) {
        const list = values.filter(v => (v !== page.sleep || PowerOptions.canSuspend) && (v !== page.hibernate || PowerOptions.canHibernate));
        if (!list.includes(current)) {
            list.push(current);
        }
        return list.map(v => ({ value: v, text: page.actionText(v) }));
    }

    // powerdevil's defaults (powerdevilsettingsdefaults.cpp) for keys nobody
    // has set yet.
    function lidAction(profile) {
        revision;
        return System.config("powerdevilrc", profile + "/SuspendAndShutdown", "LidAction", PowerOptions.canSuspend ? page.sleep : page.screenOff);
    }

    function powerButtonAction(profile) {
        revision;
        return System.config("powerdevilrc", profile + "/SuspendAndShutdown", "PowerButtonAction", page.askWhatToDo);
    }

    // The low battery profile is powerdevil's third, used below the low
    // battery level. Wild OS shows two like Windows, so the battery choice
    // covers it too and the lid never behaves differently at 9%.
    function setButtonAction(profile, key, value) {
        System.setConfig("powerdevilrc", profile + "/SuspendAndShutdown", key, value);
        if (profile === "Battery") {
            System.setConfig("powerdevilrc", "LowBattery/SuspendAndShutdown", key, value);
        }
        System.reloadPowerManagement();
        revision++;
    }

    readonly property bool lidWithScreen: {
        revision;
        return !System.config("powerdevilrc", "AC/SuspendAndShutdown", "InhibitLidActionWhenExternalMonitorPresent", true);
    }

    function setLidWithScreen(on) {
        for (const profile of ["AC", "Battery", "LowBattery"]) {
            System.setConfig("powerdevilrc", profile + "/SuspendAndShutdown", "InhibitLidActionWhenExternalMonitorPresent", !on);
        }
        System.reloadPowerManagement();
        revision++;
    }

    readonly property var dimDefaults: ({ "AC": 300, "Battery": 120 })
    readonly property var screenOffDefaults: ({ "AC": 600, "Battery": 300 })

    function dimAfter(profile) {
        revision;
        if (!System.config("powerdevilrc", profile + "/Display", "DimDisplayWhenIdle", true)) {
            return 0;
        }
        return System.config("powerdevilrc", profile + "/Display", "DimDisplayIdleTimeoutSec", dimDefaults[profile]);
    }

    function screenOffAfter(profile) {
        revision;
        if (!System.config("powerdevilrc", profile + "/Display", "TurnOffDisplayWhenIdle", true)) {
            return 0;
        }
        return System.config("powerdevilrc", profile + "/Display", "TurnOffDisplayIdleTimeoutSec", screenOffDefaults[profile]);
    }

    function setDimAfter(profile, seconds) {
        System.setConfig("powerdevilrc", profile + "/Display", "DimDisplayWhenIdle", seconds > 0);
        if (seconds > 0) {
            System.setConfig("powerdevilrc", profile + "/Display", "DimDisplayIdleTimeoutSec", seconds);
        }
        System.reloadPowerManagement();
        revision++;
    }

    function dimTimeouts(current) {
        const list = [{ value: 30, text: "30 seconds" }].concat([1, 2, 3, 5, 10, 15, 20, 30].map(m => ({ value: m * 60, text: Format.plural(m, "minute", "minutes") })));
        if (current > 0 && !list.some(o => o.value === current)) {
            list.push({ value: current, text: current < 60 ? Format.plural(current, "second", "seconds") : Format.plural(Math.round(current / 60), "minute", "minutes") });
            list.sort((a, b) => a.value - b.value);
        }
        return [{ value: 0, text: "Never" }].concat(list);
    }

    readonly property int criticalAction: {
        revision;
        const fallback = PowerOptions.canHibernate ? page.hibernate : PowerOptions.canSuspend ? page.sleep : page.doNothing;
        return System.config("powerdevilrc", "BatteryManagement", "BatteryCriticalAction", fallback);
    }

    readonly property int lowLevel: {
        revision;
        return System.config("powerdevilrc", "BatteryManagement", "BatteryLowLevel", 10);
    }

    readonly property int criticalLevel: {
        revision;
        return System.config("powerdevilrc", "BatteryManagement", "BatteryCriticalLevel", 5);
    }

    function setBattery(key, value) {
        System.setConfig("powerdevilrc", "BatteryManagement", key, value);
        System.reloadPowerManagement();
        revision++;
    }

    // Critical has to stay below low, or the low battery warning never shows.
    function criticalLevels(current) {
        const list = [2, 3, 4, 5, 6, 7, 8, 9, 10, 15].filter(n => n < page.lowLevel);
        if (!list.includes(current)) {
            list.push(current);
            list.sort((a, b) => a - b);
        }
        return list.map(n => ({ value: n, text: n + "%" }));
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: PowerOptions.lidPresent ? "Lid and power button" : "Power button"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                Repeater {
                    model: page.profiles

                    Column {
                        id: block
                        required property var modelData
                        required property int index
                        width: parent.width

                        Rectangle {
                            visible: block.index > 0
                            width: parent.width
                            height: 1
                            color: UI.Style.divider
                        }

                        UI.SettingRow {
                            id: powerRow
                            readonly property string label: page.capitalise(block.modelData.prefix + "pressing the power button will")
                            iconName: "power"
                            title: label

                            UI.ComboBox {
                                anchors.verticalCenter: parent.verticalCenter
                                Accessible.name: powerRow.label
                                readonly property var choice: page.powerButtonAction(block.modelData.id)
                                model: page.actionChoices([page.doNothing, page.sleep, page.hibernate, page.shutDown, page.lock, page.askWhatToDo, page.screenOff], choice)
                                currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                                onActivated: index => page.setButtonAction(block.modelData.id, "PowerButtonAction", model[index].value)
                            }
                        }

                        UI.SettingRow {
                            id: lidRow
                            readonly property string label: page.capitalise(block.modelData.prefix + "closing the lid will")
                            visible: PowerOptions.lidPresent
                            iconName: "computer"
                            title: label

                            UI.ComboBox {
                                anchors.verticalCenter: parent.verticalCenter
                                Accessible.name: lidRow.label
                                readonly property var choice: page.lidAction(block.modelData.id)
                                model: page.actionChoices([page.doNothing, page.sleep, page.hibernate, page.shutDown, page.lock, page.screenOff], choice)
                                currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                                onActivated: index => page.setButtonAction(block.modelData.id, "LidAction", model[index].value)
                            }
                        }
                    }
                }

                UI.SettingRow {
                    visible: PowerOptions.lidPresent
                    iconName: "monitor"
                    title: "Do this even when another screen is connected"
                    description: "When this is off, closing the lid with a monitor plugged in leaves the computer running on the monitor"
                    status: page.lidWithScreen ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: page.lidWithScreen
                        Accessible.name: "Do this even when another screen is connected"
                        onToggled: page.setLidWithScreen(checked)
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Screen dimming"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                Repeater {
                    model: page.profiles

                    UI.SettingRow {
                        id: dimRow
                        required property var modelData
                        readonly property string label: page.capitalise(modelData.prefix + "dim my screen after")
                        readonly property int dim: page.dimAfter(modelData.id)
                        readonly property int off: page.screenOffAfter(modelData.id)
                        iconName: "sun"
                        title: label
                        description: dim > 0 && off > 0 && dim >= off ? "The screen turns off before then, so it won't dim first" : ""
                        descriptionColor: UI.Style.warning

                        UI.ComboBox {
                            anchors.verticalCenter: parent.verticalCenter
                            Accessible.name: dimRow.label
                            model: page.dimTimeouts(dimRow.dim)
                            readonly property var choice: dimRow.dim
                            currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                            onActivated: index => page.setDimAfter(dimRow.modelData.id, model[index].value)
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        visible: page.hasBattery

        SectionTitle {
            text: "Battery"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    iconName: "battery"
                    title: "Critical battery level"
                    description: "Below this, Wild OS acts before the battery runs out"

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "Critical battery level"
                        readonly property var choice: page.criticalLevel
                        model: page.criticalLevels(choice)
                        currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                        onActivated: index => page.setBattery("BatteryCriticalLevel", model[index].value)
                    }
                }

                UI.SettingRow {
                    iconName: "alert"
                    title: "When the battery is critically low"

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "When the battery is critically low"
                        readonly property var choice: page.criticalAction
                        model: page.actionChoices([page.doNothing, page.sleep, page.hibernate, page.shutDown], choice)
                        currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                        onActivated: index => page.setBattery("BatteryCriticalAction", model[index].value)
                    }
                }
            }
        }
    }
}
