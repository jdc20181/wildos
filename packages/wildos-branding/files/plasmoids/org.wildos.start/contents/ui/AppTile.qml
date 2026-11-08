pragma ComponentBehavior: Bound

import QtQuick
import "code/helpers.js" as Helpers
import WildOS.UI as UI

// One pinned app: a 42px icon over a 12px label in a 92px cell.
Item {
    id: tile

    required property int index
    required property var model
    required property NavGrid grid

    readonly property string label: Helpers.appName(model.favoriteId, model.display)

    width: grid.cellWidth - grid.gap
    height: grid.rowHeight

    Accessible.role: Accessible.Button
    Accessible.name: label

    Rectangle {
        anchors.fill: parent
        radius: Style.radiusItem
        color: mouse.pressed ? UI.Style.surfacePressed : mouse.containsMouse ? UI.Style.surfaceHover : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 8

        AppIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 42
            height: 42
            source: tile.model.decoration
        }

        UI.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, tile.width)
            horizontalAlignment: Text.AlignHCenter
            size: 12
            text: tile.label
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: tile.grid.currentIndex = tile.index
        onClicked: event => {
            if (event.button === Qt.RightButton) {
                tile.grid.menuRequested(tile.index, tile, event.x, event.y);
            } else {
                tile.grid.activated(tile.index);
            }
        }
        onPressAndHold: event => tile.grid.menuRequested(tile.index, tile, event.x, event.y)
    }

    UI.FocusRing {
        visible: tile.GridView.isCurrentItem && tile.grid.activeFocus && Nav.keyboard
    }
}
