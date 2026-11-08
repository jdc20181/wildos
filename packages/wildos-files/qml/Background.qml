import QtQuick
import WildOS.Files

// The empty space behind the items: a click clears the selection, a drag
// draws a selection box, a right click gives the folder's menu, and files
// dropped here land in the folder.
Item {
    id: bg

    required property var view
    required property Item target
    // Maps a rectangle in this item's coordinates to the rows inside it.
    property var rowsIn: function(rect) { return []; }

    MouseArea {
        id: area
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        preventStealing: true
        property point origin
        property bool banding: false
        property bool additive: false
        property var before: []

        onPressed: m => {
            bg.view.keyboardUsed = false;
            bg.view.focusContent();
            if (m.button === Qt.RightButton) {
                return;
            }
            origin = Qt.point(m.x, m.y);
            additive = (m.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) !== 0;
            if (!additive) {
                bg.view.folder.clearSelection();
            }
            banding = false;
        }
        onPositionChanged: m => {
            if (!pressed || (m.buttons & Qt.LeftButton) === 0) {
                return;
            }
            if (!banding && Math.abs(m.x - origin.x) + Math.abs(m.y - origin.y) > 4) {
                banding = true;
            }
            if (banding) {
                const x = Math.max(0, Math.min(origin.x, m.x));
                const y = Math.max(0, Math.min(origin.y, m.y));
                const w = Math.min(width, Math.max(origin.x, m.x)) - x;
                const h = Math.min(height, Math.max(origin.y, m.y)) - y;
                band.x = x;
                band.y = y;
                band.width = w;
                band.height = h;
                bg.view.folder.selectRows(bg.rowsIn(Qt.rect(x, y, w, h)), additive);
            }
        }
        onReleased: banding = false
        onClicked: m => {
            if (m.button === Qt.RightButton) {
                bg.view.folder.clearSelection();
                bg.view.openFolderMenu(bg, m.x, m.y);
            }
        }
    }

    Rectangle {
        id: band
        visible: area.banding
        color: Metrics.bandFill
        border.width: 1
        border.color: Metrics.bandBorder
        radius: 2
    }

    DropArea {
        anchors.fill: parent
        enabled: bg.view.folder && (bg.view.folder.writable || bg.view.folder.kind === "bin")
        onEntered: drag => drag.accept(drag.proposedAction)
        onDropped: drop => bg.view.dropUrls(drop, bg.view.folder.url)
    }
}
