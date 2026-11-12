import QtQuick
import QtQuick.Templates as T
import WildOS.Music
import WildOS.UI as UI

// A page that is mostly songs: whatever the page puts on top (an album's
// cover and buttons, a heading), the column names, then the rows, all
// scrolling together. Rows sit 14px in from the page and the column names
// 28px, as on the board.
ListView {
    id: list

    property var tracks: []
    // "track" numbers rows by their track number, "index" counts them.
    property string numbering: "index"
    property bool showAlbum: false
    property Component heading
    property bool showColumns: tracks.length > 0
    property Component empty

    signal playAt(int index)
    signal menuAt(int index, Item row, real x, real y)

    readonly property real artistWidth: Math.min(200, Math.max(110, (width - 28 - 200) * 0.22))

    clip: true
    focus: true
    model: tracks
    currentIndex: -1
    boundsBehavior: Flickable.StopAtBounds
    T.ScrollBar.vertical: UI.ScrollBar {}
    bottomMargin: 16
    keyNavigationEnabled: true
    Accessible.role: Accessible.Table

    Keys.onReturnPressed: if (currentIndex >= 0) list.playAt(currentIndex)
    Keys.onEnterPressed: if (currentIndex >= 0) list.playAt(currentIndex)

    header: Column {
        width: list.width

        Loader {
            width: parent.width
            sourceComponent: list.heading
        }
        TrackHeader {
            visible: list.showColumns
            x: 28
            width: parent.width - 56
            showAlbum: list.showAlbum
            artistWidth: list.artistWidth
        }
        Item {
            width: 1
            height: 6
        }
        Loader {
            width: parent.width
            height: item ? item.implicitHeight : 0
            active: list.tracks.length === 0 && list.empty !== null
            sourceComponent: list.empty
        }
    }

    // ListView places its delegates at x 0, so the row is inset in a
    // full-width holder.
    delegate: Item {
        id: holder
        required property var modelData
        required property int index
        width: list.width
        height: trackRow.height

        TrackRow {
            id: trackRow
            x: 14
            width: list.width - 28
            track: holder.modelData
            number: list.numbering === "track" ? (holder.modelData.number ?? 0) : holder.index + 1
            showAlbum: list.showAlbum
            artistWidth: list.artistWidth
            selected: list.currentIndex === holder.index && list.activeFocus
            onPressed: {
                list.currentIndex = holder.index;
                list.forceActiveFocus();
            }
            onPlay: list.playAt(holder.index)
            onMenuRequested: (x, y) => list.menuAt(holder.index, trackRow, x, y)
        }
    }
}
