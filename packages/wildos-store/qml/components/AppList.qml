import QtQuick
import QtQuick.Layouts
import WildOS.Store

// A short list of apps in the boards' columns (three across, 34px apart,
// filled a row at a time), for the handful on Discover, a category's top
// picks or a search's swaps. Long lists use AppGrid, which only makes the
// rows in view.
Grid {
    id: list

    property var keys: []
    property int maxItems: -1
    property bool numbered: false
    // The curated note under the button ("Opens PSD files"), for swaps.
    property bool showNotes: false

    signal activated(string key)

    readonly property var shownKeys: maxItems > 0 ? keys.slice(0, maxItems) : keys
    readonly property int lastRowStart: {
        const n = shownKeys.length;
        const rem = n % columns;
        return n - (rem === 0 ? columns : rem);
    }

    Layout.fillWidth: true
    columns: width > 900 ? 3 : width > 560 ? 2 : 1
    columnSpacing: Metrics.columnGap
    rowSpacing: 0
    readonly property real cellWidth: (width - columnSpacing * (columns - 1)) / columns

    function noteOf(key) {
        const a = Catalog.app(key);
        if (!a || a.cardNote.length === 0) {
            return "";
        }
        const note = a.cardNote.replace(/^Free, /, "");
        return note.charAt(0).toUpperCase() + note.slice(1);
    }

    Repeater {
        model: list.shownKeys

        AppListRow {
            required property string modelData
            required property int index
            width: list.cellWidth
            appKey: modelData
            number: list.numbered ? index + 1 : 0
            divider: index < list.lastRowStart
            note: list.showNotes ? list.noteOf(modelData) : ""
            onActivated: key => list.activated(key)
        }
    }
}
