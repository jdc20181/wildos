import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WildOS.UI as UI

// The scrolling body of a page, with the design's content padding (6px top,
// 12px left, 32px right) and 26px between sections.
Flickable {
    id: flick

    default property alias content: column.data
    property alias spacing: column.spacing

    contentWidth: width
    contentHeight: column.implicitHeight + Metrics.contentTop + 28
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: UI.ScrollBar {}

    ColumnLayout {
        id: column
        x: Metrics.contentLeft
        y: Metrics.contentTop
        width: flick.width - Metrics.contentLeft - Metrics.contentRight
        spacing: Metrics.sectionGap
    }
}
