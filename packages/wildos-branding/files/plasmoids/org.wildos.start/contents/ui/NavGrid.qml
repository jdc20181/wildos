import QtQuick

// A grid that moves with the arrow keys and hands focus to its neighbours
// when the selection runs off the top or bottom edge.
GridView {
    id: grid

    property int columns: 1
    property int limit: 0
    property int maxRows: 3
    property real gap: 2
    property real rowHeight: 58

    readonly property int shownCount: limit > 0 ? Math.min(count, limit) : count
    readonly property int rows: Math.ceil(shownCount / columns)

    signal exitUp()
    signal exitDown()
    signal activated(int index)
    signal menuRequested(int index, Item item, real x, real y)

    // Callers size the view one gap wider than the content so the last
    // column fits; each delegate then leaves the gap on its right.
    cellWidth: width / columns
    cellHeight: rowHeight + gap
    implicitHeight: Math.max(rows, 1) > maxRows ? maxRows * cellHeight - gap : Math.max(rows, 1) * cellHeight - gap
    interactive: rows > maxRows
    clip: rows > maxRows
    boundsBehavior: Flickable.StopAtBounds
    keyNavigationEnabled: false
    highlightFollowsCurrentItem: false
    currentIndex: -1
    reuseItems: false

    function enter(fromBelow: bool): void {
        if (shownCount === 0) {
            return;
        }
        if (currentIndex < 0 || currentIndex >= shownCount) {
            currentIndex = fromBelow ? (rows - 1) * columns : 0;
        }
        forceActiveFocus(fromBelow ? Qt.BacktabFocusReason : Qt.TabFocusReason);
    }

    onActiveFocusChanged: {
        if (activeFocus && (currentIndex < 0 || currentIndex >= shownCount) && shownCount > 0) {
            currentIndex = 0;
        }
    }

    Keys.onPressed: event => {
        const current = Math.max(currentIndex, 0);
        const column = current % columns;
        switch (event.key) {
        case Qt.Key_Left:
            if (current > 0) {
                currentIndex = current - 1;
            }
            break;
        case Qt.Key_Right:
            if (current < shownCount - 1) {
                currentIndex = current + 1;
            }
            break;
        case Qt.Key_Up:
            if (current - columns >= 0) {
                currentIndex = current - columns;
            } else {
                exitUp();
            }
            break;
        case Qt.Key_Down:
            if (current + columns < shownCount) {
                currentIndex = current + columns;
            } else if (Math.floor(current / columns) < rows - 1) {
                currentIndex = shownCount - 1;
            } else {
                exitDown();
            }
            break;
        case Qt.Key_Home:
            currentIndex = 0;
            break;
        case Qt.Key_End:
            currentIndex = shownCount - 1;
            break;
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
            if (currentIndex >= 0) {
                activated(currentIndex);
            }
            break;
        case Qt.Key_Menu:
            if (currentItem) {
                menuRequested(currentIndex, currentItem, currentItem.width / 2, currentItem.height / 2);
            }
            break;
        default:
            if (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier) && currentItem) {
                menuRequested(currentIndex, currentItem, currentItem.width / 2, currentItem.height / 2);
                break;
            }
            return;
        }
        Nav.keyboard = true;
        positionViewAtIndex(currentIndex, GridView.Contain);
        event.accepted = true;
    }
}
