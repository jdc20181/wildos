import QtQuick
import QtQuick.Shapes
import WildOS.UI as UI

// The design's Wi-Fi icon with its arcs dimmed to show signal strength.
// bars is 0 to 3; -1 draws every arc faint, for "not connected".
Item {
    id: glyph

    property int bars: 3
    property color color: UI.Style.iconInk
    property real strokeWidth: 2
    readonly property color faint: Qt.rgba(color.r, color.g, color.b, color.a * 0.32)

    implicitWidth: 18
    implicitHeight: 18

    Shape {
        width: 24
        height: 24
        preferredRendererType: Shape.CurveRenderer
        transform: Scale { xScale: glyph.width / 24; yScale: glyph.height / 24 }

        ShapePath {
            strokeColor: glyph.bars >= 3 ? glyph.color : glyph.faint
            strokeWidth: glyph.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: Icons.wifiArcOuter }
        }
        ShapePath {
            strokeColor: glyph.bars >= 2 ? glyph.color : glyph.faint
            strokeWidth: glyph.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: Icons.wifiArcMiddle }
        }
        ShapePath {
            strokeColor: glyph.bars >= 1 ? glyph.color : glyph.faint
            strokeWidth: glyph.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: Icons.wifiArcInner }
        }
        ShapePath {
            strokeColor: "transparent"
            fillColor: glyph.bars >= 0 ? glyph.color : glyph.faint
            PathSvg { path: Icons.wifiDotTray }
        }
    }
}
