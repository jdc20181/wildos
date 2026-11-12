import QtQuick
import QtQuick.Layouts
import WildOS.Mail
import WildOS.UI as UI
import "Formats.js" as Formats

// Picking a day, and a time if one is wanted, for a task's due date or its
// reminder: the small month from Calendar with the times under it.
UI.Dialog {
    id: picker

    // A reminder needs a time; a due date may be a whole day.
    property bool needsTime: false
    property date day: new Date()
    property int minutes: 9 * 60

    signal chosen(date when, bool allDay)

    readonly property var times: {
        const out = needsTime ? [] : [{ value: -1, text: qsTr("No time") }];
        for (let m = 0; m < 24 * 60; m += 15) {
            out.push({ value: m, text: Formats.time(new Date(2000, 0, 1, Math.floor(m / 60), m % 60)) });
        }
        return out;
    }

    function ask(heading, when, timed) {
        title = heading;
        const d = when && !isNaN(when.getTime()) ? when : new Date();
        day = new Date(d.getFullYear(), d.getMonth(), d.getDate());
        mini.month = new Date(d.getFullYear(), d.getMonth(), 1);
        const valid = when && !isNaN(when.getTime());
        if (valid && timed) {
            minutes = d.getHours() * 60 + d.getMinutes();
        } else {
            minutes = needsTime ? 9 * 60 : -1;
        }
        const i = times.findIndex(t => t.value === minutes);
        timeBox.currentIndex = i >= 0 ? i : 0;
        open();
    }

    MiniMonth {
        id: mini
        Layout.fillWidth: true
        showBusy: false
        rangeFrom: picker.day
        rangeTo: new Date(picker.day.getFullYear(), picker.day.getMonth(), picker.day.getDate() + 1)
        onPicked: function (d) {
            picker.day = d;
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        UI.Label {
            Layout.fillWidth: true
            text: Formats.longDate(picker.day)
            size: 13
            elide: Text.ElideRight
        }
        UI.ComboBox {
            id: timeBox
            implicitWidth: 120
            model: picker.times
            onActivated: function (i) {
                picker.minutes = picker.times[i].value;
            }
        }
    }

    buttons: [
        UI.Button {
            text: qsTr("Cancel")
            onClicked: picker.close()
        },
        UI.Button {
            kind: "primary"
            text: qsTr("Save")
            onClicked: {
                const d = picker.day;
                const allDay = picker.minutes < 0;
                const m = Math.max(0, picker.minutes);
                picker.chosen(new Date(d.getFullYear(), d.getMonth(), d.getDate(), Math.floor(m / 60), m % 60), allDay);
                picker.close();
            }
        }
    ]
}
