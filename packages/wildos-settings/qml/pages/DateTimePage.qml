import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Date and time, after Windows 11: set the time automatically, the time
// zone, and setting the clock by hand when automatic time is off. All of it
// goes through timedated, which asks for the administrator password when
// the account needs one.
PageScroll {
    id: page

    property date now: new Date()
    property string error

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: page.now = new Date()
    }

    Connections {
        target: Clock
        function onFailed(reason) {
            page.error = reason;
            autoToggle.checked = Qt.binding(() => Clock.automatic);
        }
        function onChanged() {
            page.error = "";
        }
    }

    UI.Card {
        Layout.fillWidth: true
        implicitHeight: 112

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 24

            UI.Label {
                text: Format.time(page.now)
                size: 40
                font.weight: Font.Light
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                UI.Label {
                    Layout.fillWidth: true
                    text: Format.longDate(page.now) + " " + page.now.getFullYear()
                    size: 15
                    font.weight: Font.DemiBold
                }

                UI.Label {
                    Layout.fillWidth: true
                    text: Clock.automatic ? (Clock.synchronised ? "Set automatically from the internet" : "Set automatically when the computer is online")
                        : "Set by hand"
                    color: UI.Style.inkMuted
                }
            }
        }
    }

    StatusCard {
        visible: page.error.length > 0 || !Clock.available
        iconName: "warning"
        warning: true
        title: Clock.available ? "The clock was not changed" : "The clock cannot be changed here"
        description: Clock.available ? page.error : "The service that sets the date and time is not running on this computer."
    }

    UI.Card {
        Layout.fillWidth: true
        enabled: Clock.available

        Column {
            width: parent.width

            UI.SettingRow {
                iconName: "clock"
                title: "Set time automatically"
                description: Clock.canAutomatic ? "Keeps the clock right using a time server on the internet"
                    : "No time service is installed, so the clock is set by hand"
                enabled: Clock.canAutomatic
                status: Clock.automatic ? "On" : "Off"

                UI.Toggle {
                    id: autoToggle
                    anchors.verticalCenter: parent.verticalCenter
                    checked: Clock.automatic
                    Accessible.name: "Set time automatically"
                    onToggled: Clock.setAutomatic(checked)
                }
            }

            UI.SettingRow {
                iconName: "globe"
                title: "Time zone"

                UI.ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    Accessible.name: "Time zone"
                    model: Clock.timeZones()
                    readonly property string choice: Clock.timeZone
                    currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                    onActivated: index => Clock.setTimeZone(model[index].value)
                }
            }

            UI.SettingRow {
                iconName: "pencil"
                title: "Set the date and time manually"
                description: Clock.automatic ? "Turn off automatic time first" : ""
                enabled: !Clock.automatic

                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Change"
                    onClicked: changeDialog.open()
                }
            }
        }
    }

    UI.Dialog {
        id: changeDialog

        readonly property var dateParts: /^(\d{1,2})\/(\d{1,2})\/(\d{4})$/.exec(dateField.text.trim())
        readonly property var timeParts: /^(\d{1,2}):(\d{2})$/.exec(timeField.text.trim())
        readonly property bool valid: dateParts !== null && timeParts !== null
            && +timeParts[1] < 24 && +timeParts[2] < 60 && +dateParts[2] >= 1 && +dateParts[2] <= 12 && +dateParts[1] >= 1 && +dateParts[1] <= 31

        title: "Change the date and time"
        text: "Type the date as day, month and year, and the time on the 24-hour clock."
        onOpened: {
            const d = new Date();
            dateField.text = Format.shortDate(d);
            timeField.text = Format.time(d);
            dateField.forceActiveFocus();
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            UI.TextField {
                id: dateField
                Layout.fillWidth: true
                placeholderText: "Date, for example 07/11/2026"
                error: text.length > 0 && changeDialog.dateParts === null
                onAccepted: if (changeDialog.valid) changeButton.clicked()
            }

            UI.TextField {
                id: timeField
                Layout.preferredWidth: 110
                placeholderText: "Time"
                error: text.length > 0 && changeDialog.timeParts === null
                onAccepted: if (changeDialog.valid) changeButton.clicked()
            }
        }

        buttons: [
            UI.Button {
                id: changeButton
                text: "Change"
                kind: "primary"
                enabled: changeDialog.valid
                onClicked: {
                    const d = changeDialog.dateParts;
                    const t = changeDialog.timeParts;
                    Clock.setDateTime(+d[1], +d[2], +d[3], +t[1], +t[2]);
                    changeDialog.close();
                }
            },
            UI.Button {
                text: "Cancel"
                onClicked: changeDialog.close()
            }
        ]
    }
}
