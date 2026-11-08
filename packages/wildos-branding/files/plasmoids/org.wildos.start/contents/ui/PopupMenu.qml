import QtQuick
import WildOS.UI as UI

// Start's menus (the app and file commands, the account menu and the power
// menu) are the shared Wild OS menu, opened inside Start's window and placed
// against the button or item they belong to. Start draws its rows 40 px
// tall, as the PowerMenu artboard does.
UI.Menu {
    id: popup

    // Each entry: { text, icon, run } or { separator: true }; see UI.Menu.
    property Item returnFocus: null
    property real menuWidth: 250
    property int rowHeight: 40

    minimumWidth: menuWidth
    maximumWidth: menuWidth

    onEntriesChanged: Qt.callLater(tallRows)

    function tallRows(): void {
        for (let i = 0; i < count; ++i) {
            const item = itemAt(i);
            if (item && item.iconName !== undefined) {
                item.implicitHeight = popup.rowHeight;
            }
        }
    }

    // Hangs above a button with its right edge rightOverhang past the
    // button's right edge (the power menu).
    function openAbove(anchorItem: Item, rightOverhang: real): void {
        const point = anchorItem.mapToItem(parent, anchorItem.width + rightOverhang - menuWidth, -6);
        x = point.x;
        y = Qt.binding(() => point.y - height);
        show(anchorItem);
    }

    // Hangs above a button, starting xOffset from its left edge (the account
    // menu).
    function openAboveAt(anchorItem: Item, xOffset: real): void {
        const point = anchorItem.mapToItem(parent, xOffset, -6);
        x = point.x;
        y = Qt.binding(() => point.y - height);
        show(anchorItem);
    }

    // Opens at a point on an item, where it was right-clicked.
    function openAt(anchorItem: Item, px: real, py: real): void {
        const point = anchorItem.mapToItem(parent, px, py);
        x = point.x;
        y = point.y;
        show(anchorItem);
    }

    function show(anchorItem: Item): void {
        returnFocus = anchorItem;
        open();
    }

    onClosed: {
        if (returnFocus) {
            returnFocus.forceActiveFocus();
        }
    }
}
