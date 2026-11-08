pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts
import org.kde.kitemmodels as KItemModels
import WildOS.UI as UI

// The design's notifications card: a "Notifications" heading with "Clear
// all", then the notifications newest first, 10 px apart. It scrolls when
// there are more than the screen has room for.
ColumnLayout {
    id: card

    required property var clock
    property real maxHeight: 600
    property real now: Date.now()

    readonly property var history: clock.history
    readonly property bool empty: list.count === 0

    // Newest at the top. Plasma's model keeps arrival order here, so the
    // card sorts by the time each notification was sent.
    KItemModels.KSortFilterProxyModel {
        id: newestFirst
        sourceModel: card.history
        sortRoleName: "created"
        sortOrder: Qt.DescendingOrder
    }

    // Every button in reading order, for the flyout's keyboard focus.
    function stops(): var {
        if (card.empty) {
            return [dndButton, settingsLink];
        }
        let result = clearAll.visible ? [clearAll] : [];
        for (let i = 0; i < list.count; i++) {
            const item = list.itemAt(i);
            if (item) {
                result = result.concat(item.stops());
            }
        }
        return result;
    }

    // Scrolls the list so the keyboard's button is in view.
    function reveal(button: Item): void {
        let inside = button;
        while (inside && inside !== column) {
            inside = inside.parent;
        }
        if (!inside || !scroller.interactive) {
            return;
        }
        const top = button.mapToItem(column, 0, 0).y;
        const bottom = top + button.height;
        if (top < scroller.contentY) {
            scroller.contentY = Math.max(0, top - 8);
        } else if (bottom > scroller.contentY + scroller.height) {
            scroller.contentY = Math.min(scroller.contentHeight - scroller.height, bottom - scroller.height + 8);
        }
    }

    spacing: 10

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 4
        Layout.rightMargin: 4
        spacing: 8

        UI.Label {
            Layout.fillWidth: true
            text: "Notifications"
            // The empty card is EmptyStates.dc's (15 px); the list is
            // Flyouts.dc's (14 px).
            size: card.empty ? 15 : 14
            font.weight: Font.DemiBold
            Accessible.role: Accessible.Heading
        }

        // EmptyStates.dc: with nothing to clear, the corner holds do not
        // disturb instead, quiet until it is on.
        CardButton {
            id: dndButton
            visible: card.empty
            Layout.preferredWidth: dndLabel.implicitWidth + 12
            Layout.preferredHeight: 20
            Layout.rightMargin: -6
            fillHeight: 24
            radius: 6
            hoverColor: "transparent"
            accessibleName: card.clock.doNotDisturb ? "Turn off do not disturb" : "Turn on do not disturb"
            Accessible.role: Accessible.CheckBox
            Accessible.checked: card.clock.doNotDisturb
            onClicked: card.clock.setDoNotDisturb(!card.clock.doNotDisturb)

            UI.Label {
                id: dndLabel
                anchors.centerIn: parent
                text: card.clock.doNotDisturb ? "Do not disturb is on" : "Do not disturb"
                color: card.clock.doNotDisturb ? UI.Style.accentText : UI.Style.inkSubtle
                font.underline: dndButton.hovered
            }

            MouseArea {
                anchors.centerIn: parent
                width: parent.width
                height: 44
                onClicked: dndButton.activate()
            }
        }

        CardButton {
            id: clearAll
            visible: !card.empty
            Layout.preferredWidth: clearLabel.implicitWidth + 12
            Layout.preferredHeight: 20
            Layout.rightMargin: -6
            fillHeight: 24
            radius: 6
            hoverColor: "transparent"
            accessibleName: "Clear all notifications"
            onClicked: card.clock.clearAll()

            UI.Label {
                id: clearLabel
                anchors.centerIn: parent
                text: "Clear all"
                size: 12
                color: UI.Style.accentText
                font.underline: clearAll.hovered
            }

            // A finger-sized target around the short link.
            MouseArea {
                anchors.centerIn: parent
                width: parent.width
                height: 44
                onClicked: clearAll.activate()
            }
        }
    }

    Flickable {
        id: scroller
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(column.implicitHeight, Math.max(120, card.maxHeight - 30))
        visible: !card.empty
        contentHeight: column.implicitHeight
        // Only clipped when it scrolls, so focus rings drawn just outside the
        // cards are not cut off.
        clip: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        T.ScrollBar.vertical: UI.ScrollBar {}

        Column {
            id: column
            width: scroller.width
            spacing: 10

            Repeater {
                id: list
                model: newestFirst

                NotificationItem {
                    required property var model
                    required property int index

                    width: column.width
                    notification: model
                    now: card.now
                    function sourceIndex(): var {
                        return newestFirst.mapToSource(newestFirst.index(index, 0));
                    }

                    onDefaultActionInvoked: card.clock.invokeDefault(card.history, sourceIndex(), !!model.resident)
                    onActionInvoked: name => card.clock.invokeAction(card.history, sourceIndex(), name, !!model.resident)
                    onCloseRequested: card.clock.closeNotification(card.history, sourceIndex())
                    onKillJobRequested: card.history.killJob(sourceIndex())
                    onLinkActivated: link => {
                        Qt.openUrlExternally(link);
                        card.clock.expanded = false;
                    }
                }
            }
        }
    }

    // EmptyStates.dc: a calm answer instead of a blank card, and where to
    // change what may notify.
    ColumnLayout {
        visible: card.empty
        Layout.fillWidth: true
        Layout.topMargin: 14
        Layout.bottomMargin: 2
        spacing: 12

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
            radius: 32
            color: UI.Style.accentSoft

            UI.Glyph {
                anchors.centerIn: parent
                width: 30
                height: 30
                path: "M6 16V11a6 6 0 0 1 12 0v5l1.5 2h-15zM10 20a2 2 0 0 0 4 0"
                strokeWidth: 1.9
                color: UI.Style.accentText
            }
        }

        UI.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "You're all caught up"
            size: 15
            font.weight: Font.DemiBold
        }

        UI.Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(280, card.width)
            horizontalAlignment: Text.AlignHCenter
            text: "New notifications from your apps will appear here."
            color: UI.Style.inkMuted
            lineHeight: 1.5
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        CardButton {
            id: settingsLink
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4
            Layout.preferredWidth: settingsLabel.implicitWidth + 12
            Layout.preferredHeight: 24
            radius: 6
            hoverColor: "transparent"
            accessibleName: "Notification settings"
            Accessible.role: Accessible.Link
            onClicked: card.clock.openSettings("notifications")

            UI.Label {
                id: settingsLabel
                anchors.centerIn: parent
                text: "Notification settings"
                color: UI.Style.accentText
                font.underline: settingsLink.hovered
            }
        }
    }
}
