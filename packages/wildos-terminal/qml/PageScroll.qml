import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import WildOS.Terminal
import WildOS.UI as UI

// The scrolling body every Wild OS settings page sits in, with the design's
// content margins and the 22px gap between blocks.
Flickable {
    id: flick

    default property alias content: column.data
    property int maxContentWidth: 1000

    contentWidth: width
    contentHeight: column.implicitHeight + 32
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    T.ScrollBar.vertical: UI.ScrollBar {}

    ColumnLayout {
        id: column
        x: Metrics.contentLeft
        y: 4
        width: Math.min(flick.width - Metrics.contentLeft - Metrics.contentRight, flick.maxContentWidth)
        spacing: Metrics.sectionGap
    }
}
