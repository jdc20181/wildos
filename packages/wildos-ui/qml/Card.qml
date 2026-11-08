import QtQuick
import QtQuick.Effects
import QtQuick.Templates as T

// A raised group inside a window: radius 12, surface-raised 0.05 with a
// 0.07 edge (Settings.dc, Store.dc tiles, WinApp status). From
// wildos-settings Card and RowGroup. Rows inside are masked to the rounded
// shape, so the first and last row's hover no longer paints square corners
// over the edge (RowGroup clipped to a rectangle). `padding` defaults to 0
// for lists of rows; use 16 or 18 for content. `tinted` is the accent
// banner (protected, status cards).
T.Pane {
    id: control

    property real radius: Style.radiusMd
    property bool tinted: false
    property bool clipContent: true

    padding: 0
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: implicitContentHeight + topPadding + bottomPadding

    background: Rectangle {
        radius: control.radius
        color: control.tinted ? Style.accentSoft : Style.surfaceRaised
        border.width: 1
        border.color: control.tinted ? Style.accentBorder : Style.borderSubtle

        Item {
            id: mask
            anchors.fill: parent
            visible: false
            layer.enabled: true

            Rectangle {
                anchors.fill: parent
                radius: control.radius
            }
        }
    }

    layer.enabled: clipContent
    layer.effect: MultiEffect {
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }
}
