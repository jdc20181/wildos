pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// The design's calendar card: today's date as the heading, the month shown
// beneath it with previous and next buttons, a Monday-first month grid with
// today in the accent, and a line along the foot about the day chosen.
// Wild OS has no calendar of events, so choosing a day says how far away
// it is, the question a calendar flyout mostly answers, rather than
// claiming the day is free.
ColumnLayout {
    id: card

    required property date today

    property int year: today.getFullYear()
    property int month: today.getMonth()
    readonly property bool showingToday: year === today.getFullYear() && month === today.getMonth()
    // The day picked in the grid; null means today.
    property var chosen: null

    function stops(): var {
        return showingToday ? [previous, next] : [todayButton, previous, next];
    }

    readonly property var locale: Qt.locale("en_GB")

    // Six weeks from the Monday on or before the first, so the card keeps
    // one height from month to month.
    readonly property var days: {
        const first = new Date(year, month, 1);
        const offset = (first.getDay() + 6) % 7;
        const list = [];
        for (let i = 0; i < 42; i++) {
            const day = new Date(year, month, 1 - offset + i);
            list.push({
                date: day,
                n: day.getDate(),
                inMonth: day.getMonth() === month,
                isToday: day.getFullYear() === today.getFullYear() && day.getMonth() === today.getMonth()
                    && day.getDate() === today.getDate(),
                label: locale.toString(day, "dddd d MMMM yyyy")
            });
        }
        return list;
    }

    function step(months: int): void {
        const target = new Date(year, month + months, 1);
        year = target.getFullYear();
        month = target.getMonth();
    }

    function goToToday(): void {
        year = today.getFullYear();
        month = today.getMonth();
        chosen = null;
    }

    function sameDay(a: var, b: var): bool {
        return a !== null && b !== null && a.getFullYear() === b.getFullYear()
            && a.getMonth() === b.getMonth() && a.getDate() === b.getDate();
    }

    // ISO weeks, as Windows numbers them in the UK.
    function weekNumber(day: var): int {
        const d = new Date(Date.UTC(day.getFullYear(), day.getMonth(), day.getDate()));
        const weekday = (d.getUTCDay() + 6) % 7;
        d.setUTCDate(d.getUTCDate() - weekday + 3);
        const firstThursday = new Date(Date.UTC(d.getUTCFullYear(), 0, 4));
        return 1 + Math.round(((d - firstThursday) / 86400000 - 3 + (firstThursday.getUTCDay() + 6) % 7) / 7);
    }

    readonly property string footer: {
        const day = chosen !== null ? chosen : today;
        const start = new Date(today.getFullYear(), today.getMonth(), today.getDate());
        const days = Math.round((new Date(day.getFullYear(), day.getMonth(), day.getDate()) - start) / 86400000);
        const when = days === 0 ? "Today" : days === 1 ? "Tomorrow" : days === -1 ? "Yesterday"
            : days > 0 ? "In " + days + " days" : days * -1 + " days ago";
        if (days === 0) {
            return "Today, week " + weekNumber(day);
        }
        return locale.toString(day, "dddd d MMMM") + ", " + when.charAt(0).toLowerCase() + when.slice(1);
    }

    spacing: 12

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            UI.Label {
                Layout.fillWidth: true
                text: card.locale.toString(card.today, "dddd d MMMM")
                size: 14
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
            }

            UI.Label {
                Layout.fillWidth: true
                text: card.locale.toString(new Date(card.year, card.month, 1), "MMMM yyyy")
                size: 12
                color: UI.Style.inkMuted
            }
        }

        CardButton {
            id: todayButton
            visible: !card.showingToday
            Layout.preferredWidth: todayLabel.implicitWidth + 20
            Layout.preferredHeight: 44
            fillHeight: 32
            radius: 8
            accessibleName: "Back to this month"
            onClicked: card.goToToday()

            UI.Label {
                id: todayLabel
                anchors.centerIn: parent
                text: "Today"
                size: 12
                color: UI.Style.accentText
                font.underline: todayButton.hovered
            }
        }

        RowLayout {
            spacing: 2

            CardButton {
                id: previous
                Layout.preferredWidth: 32
                Layout.preferredHeight: 44
                fillHeight: 32
                accessibleName: "Previous month"
                onClicked: card.step(-1)

                UI.Glyph {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    path: "M15 6l-6 6 6 6"
                    strokeWidth: 2.2
                }
            }

            CardButton {
                id: next
                Layout.preferredWidth: 32
                Layout.preferredHeight: 44
                fillHeight: 32
                accessibleName: "Next month"
                onClicked: card.step(1)

                UI.Glyph {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    path: "M9 6l6 6-6 6"
                    strokeWidth: 2.2
                }
            }
        }
    }

    GridLayout {
        id: grid
        Layout.fillWidth: true
        // The next month rises gently into place and the previous one drops
        // in from above, as the Windows calendar does.
        transform: UI.Reveal {
            item: grid
            key: card.year * 12 + card.month
            directional: true
            dy: UI.Style.travelSmall
            duration: UI.Style.motionFast
        }
        // The buttons above are 44 px for touch; the design's gap is to their
        // 32 px fill.
        Layout.topMargin: -6
        columns: 7
        columnSpacing: 2
        rowSpacing: 2
        Accessible.role: Accessible.Table
        Accessible.name: card.locale.toString(new Date(card.year, card.month, 1), "MMMM yyyy")

        Repeater {
            model: ["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"]

            UI.Label {
                required property string modelData
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.preferredHeight: 26
                horizontalAlignment: Text.AlignHCenter
                text: modelData
                size: 11
                font.weight: Font.DemiBold
                color: UI.Style.inkMuted
            }
        }

        Repeater {
            model: card.days

            MouseArea {
                id: cell
                required property var modelData
                readonly property bool picked: card.sameDay(card.chosen, modelData.date) && !modelData.isToday
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.preferredHeight: 36
                hoverEnabled: true
                Accessible.role: Accessible.Cell
                Accessible.name: modelData.label + (modelData.isToday ? ", today" : "")
                Accessible.selected: picked
                onClicked: {
                    card.chosen = modelData.isToday ? null : modelData.date;
                    if (!modelData.inMonth) {
                        card.year = modelData.date.getFullYear();
                        card.month = modelData.date.getMonth();
                    }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: 36
                    radius: 18
                    color: cell.modelData.isToday ? (cell.pressed ? UI.Style.accentPressed : cell.containsMouse ? UI.Style.accentHover : UI.Style.accent)
                        : cell.pressed ? UI.Style.surfacePressed
                        : cell.containsMouse ? UI.Style.surfaceHover : "transparent"
                    border.width: cell.picked ? 2 : 0
                    border.color: UI.Style.accent
                }

                UI.Label {
                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    text: cell.modelData.n
                    font.features: { "tnum": 1 }
                    font.weight: cell.modelData.isToday ? Font.DemiBold : Font.Normal
                    color: cell.modelData.isToday ? UI.Style.textOnAccent
                        : cell.modelData.inMonth ? UI.Style.ink : UI.Style.inkSubtle
                }
            }
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.topMargin: 2
        implicitHeight: footerText.implicitHeight + 12

        Rectangle {
            width: parent.width
            height: 1
            color: UI.Style.tone(0.08)
        }

        UI.Label {
            id: footerText
            y: 12
            width: parent.width
            text: card.footer
            size: 12
            color: UI.Style.inkMuted
        }
    }
}
