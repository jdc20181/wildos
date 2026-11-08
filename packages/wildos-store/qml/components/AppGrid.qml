import QtQuick
import QtQuick.Controls
import WildOS.UI as UI

// A long list of apps as a scrolling grid of compact cards, for categories
// and search results that can run to thousands of apps.
GridView {
    id: grid

    property var keys: []

    signal activated(string key)

    readonly property int columns: width > 912 ? 4 : width > 600 ? 3 : 2
    leftMargin: Metrics.contentLeft
    rightMargin: Metrics.contentRight - 12
    topMargin: Metrics.contentTop
    bottomMargin: 28
    cellWidth: Math.floor((width - leftMargin - rightMargin) / columns)
    cellHeight: probe.implicitHeight + 12
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    model: keys
    reuseItems: true
    ScrollBar.vertical: UI.ScrollBar {}

    // Every cell is as tall as one card at the current text size.
    AppTile {
        id: probe
        visible: false
        width: 200
    }

    delegate: Item {
        required property string modelData
        width: grid.cellWidth
        height: grid.cellHeight

        AppTile {
            width: parent.width - 12
            height: grid.cellHeight - 12
            appKey: parent.modelData
            onActivated: key => grid.activated(key)
        }
    }
}
