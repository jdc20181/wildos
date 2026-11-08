import QtQuick
import QtQuick.Layouts

// A grid of compact app cards, four across with 12px gaps as on Home.
Grid {
    id: grid

    property var keys: []
    property int maxItems: -1

    signal activated(string key)

    Layout.fillWidth: true
    columns: width > 900 ? 4 : width > 600 ? 3 : 2
    columnSpacing: 12
    rowSpacing: 12
    readonly property real cellWidth: (width - columnSpacing * (columns - 1)) / columns

    Repeater {
        model: grid.maxItems > 0 ? grid.keys.slice(0, grid.maxItems) : grid.keys

        AppTile {
            required property string modelData
            width: grid.cellWidth
            appKey: modelData
            onActivated: key => grid.activated(key)
        }
    }
}
