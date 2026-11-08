import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import WildOS.Settings
import WildOS.UI as UI

// The installed wallpapers as a grid of thumbnails; the current one gets an
// accent edge and a tick. Cells are sized from the width the page gives the
// grid, never from the thumbnails, so the layout cannot feed back on itself.
Item {
    id: grid

    property string current
    property int count: 0
    // 0 picks as many columns of about 170px as fit.
    property int columns: 0
    readonly property int spacing: 10
    readonly property int columnCount: columns > 0 ? columns : Math.max(2, Math.floor((width + spacing) / 170))
    readonly property real cell: Math.max(0, (width - spacing * (columnCount - 1)) / columnCount)
    readonly property var wallpapers: {
        const all = System.wallpapers();
        return count > 0 ? all.slice(0, count) : all;
    }

    signal picked(string path)

    Layout.fillWidth: true
    implicitHeight: flow.implicitHeight

    Flow {
        id: flow
        width: grid.width
        spacing: grid.spacing

        Repeater {
            model: grid.wallpapers

            WallpaperThumb {
                required property var modelData
                width: grid.cell
                height: Math.round(grid.cell * 0.625)
                source: modelData.thumbnail
                selected: modelData.path === grid.current
                Accessible.name: modelData.name
                onClicked: grid.picked(modelData.path)
            }
        }
    }
}
