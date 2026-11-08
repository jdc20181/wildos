pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import "code/helpers.js" as Helpers
import WildOS.UI as UI

// Every installed app, A to Z by the name Start shows, grouped under its
// first letter. Kicker sorts by the desktop file name, which would file
// "Files" under D for Dolphin, so the list is sorted again here.
FocusScope {
    id: page

    required property var launcher

    property alias backButton: header.button
    property alias list: list

    signal backRequested()
    signal leaveUp()
    signal leaveDown()
    signal menuRequested(var model, int index, Item item, real x, real y, bool pinned)

    function enter(fromBelow: bool): void {
        if (list.count === 0) {
            return;
        }
        if (list.currentIndex < 0) {
            list.currentIndex = 0;
        }
        list.forceActiveFocus(fromBelow ? Qt.BacktabFocusReason : Qt.TabFocusReason);
    }

    function rebuild(): void {
        const items = [];
        for (let i = 0; i < apps.count; i++) {
            const entry = apps.objectAt(i);
            if (!entry || entry.model.isSeparator || entry.model.hasChildren) {
                continue;
            }
            const name = Helpers.appName(entry.model.favoriteId, entry.model.display);
            const first = name.charAt(0).toUpperCase();
            items.push({ name: name, row: i, letter: /[A-Z]/.test(first) ? first : "#" });
        }
        items.sort((a, b) => a.name.localeCompare(b.name));
        sorted.clear();
        for (const item of items) {
            sorted.append(item);
        }
    }

    Instantiator {
        id: apps
        model: page.launcher.appsModel
        delegate: QtObject {
            required property var model
            required property int index
        }
        onObjectAdded: rebuildTimer.restart()
        onObjectRemoved: rebuildTimer.restart()
    }

    Timer {
        id: rebuildTimer
        interval: 0
        onTriggered: page.rebuild()
    }

    ListModel {
        id: sorted
    }

    SectionHeader {
        id: header
        width: parent.width
        title: "All apps"
        buttonText: "Back"
        backward: true
        onActivated: page.backRequested()
    }

    ListView {
        id: list
        anchors.top: header.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        model: sorted
        currentIndex: -1
        keyNavigationEnabled: false
        highlightFollowsCurrentItem: false
        boundsBehavior: Flickable.StopAtBounds
        spacing: 2

        T.ScrollBar.vertical: UI.ScrollBar {}

        section.property: "letter"
        section.criteria: ViewSection.FullString
        section.delegate: Item {
            required property string section
            width: ListView.view.width
            height: 30

            UI.Label {
                x: 8
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 6
                size: 12
                font.weight: Font.DemiBold
                color: UI.Style.inkMuted
                text: parent.section
            }
        }

        onActiveFocusChanged: {
            if (activeFocus && currentIndex < 0 && count > 0) {
                currentIndex = 0;
            }
        }

        function activate(index: int): void {
            const row = sorted.get(index).row;
            if (page.launcher.appsModel.trigger(row, "", null)) {
                page.launcher.close();
            }
        }

        function requestMenu(index: int, item: Item, x: real, y: real): void {
            page.menuRequested(page.launcher.appsModel, sorted.get(index).row, item, x, y, false);
        }

        Keys.onPressed: event => {
            switch (event.key) {
            case Qt.Key_Up:
                if (currentIndex > 0) {
                    currentIndex--;
                } else {
                    page.leaveUp();
                }
                break;
            case Qt.Key_Down:
                if (currentIndex < count - 1) {
                    currentIndex++;
                } else {
                    page.leaveDown();
                }
                break;
            case Qt.Key_PageUp:
                currentIndex = Math.max(0, currentIndex - 8);
                break;
            case Qt.Key_PageDown:
                currentIndex = Math.min(count - 1, currentIndex + 8);
                break;
            case Qt.Key_Home:
                currentIndex = 0;
                break;
            case Qt.Key_End:
                currentIndex = count - 1;
                break;
            case Qt.Key_Return:
            case Qt.Key_Enter:
            case Qt.Key_Space:
                activate(currentIndex);
                break;
            case Qt.Key_Menu:
                if (currentItem) {
                    requestMenu(currentIndex, currentItem, currentItem.width / 2, currentItem.height / 2);
                }
                break;
            default:
                return;
            }
            Nav.keyboard = true;
            positionViewAtIndex(currentIndex, ListView.Contain);
            event.accepted = true;
        }

        delegate: Item {
            id: appRow

            required property int index
            required property string name
            required property int row

            readonly property var app: apps.objectAt(row) ? apps.objectAt(row).model : null

            width: ListView.view.width - 12
            height: 44

            Accessible.role: Accessible.Button
            Accessible.name: name

            Rectangle {
                anchors.fill: parent
                radius: Style.radiusItem
                color: mouse.pressed ? UI.Style.surfacePressed : mouse.containsMouse ? UI.Style.surfaceHover : "transparent"
                Behavior on color {
                    ColorAnimation { duration: UI.Style.hoverDuration }
                }
            }

            AppIcon {
                id: icon
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 28
                height: 28
                source: appRow.app ? appRow.app.decoration : ""
            }

            UI.Label {
                anchors.left: icon.right
                anchors.leftMargin: 12
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                font.weight: Font.Medium
                text: appRow.name
            }

            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onPressed: list.currentIndex = appRow.index
                onClicked: event => {
                    if (event.button === Qt.RightButton) {
                        list.requestMenu(appRow.index, appRow, event.x, event.y);
                    } else {
                        list.activate(appRow.index);
                    }
                }
                onPressAndHold: event => list.requestMenu(appRow.index, appRow, event.x, event.y)
            }

            UI.FocusRing {
                anchors.margins: -2
                radius: Style.radiusItem + 2
                visible: appRow.ListView.isCurrentItem && list.activeFocus && Nav.keyboard
            }
        }
    }
}
