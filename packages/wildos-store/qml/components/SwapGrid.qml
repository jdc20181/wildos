import QtQuick
import QtQuick.Layouts

// The "Instead of" cards, three across.
Grid {
    id: grid

    property var keys: []

    signal activated(string key)

    Layout.fillWidth: true
    columns: width > 640 ? 3 : 2
    columnSpacing: 12
    rowSpacing: 12
    readonly property real cellWidth: (width - columnSpacing * (columns - 1)) / columns

    Repeater {
        model: grid.keys

        SwapCard {
            required property string modelData
            width: grid.cellWidth
            appKey: modelData
            onActivated: key => grid.activated(key)
        }
    }
}
