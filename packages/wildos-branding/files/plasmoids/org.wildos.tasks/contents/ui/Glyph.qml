import QtQuick
import QtQuick.Shapes

// A line icon from the design, drawn from its SVG path data so it can take any
// colour and the design's exact stroke weight at the odd sizes the flyout uses.
Item {
    id: glyph

    property string path
    // Filled shapes that sit inside a line icon: dots, the battery level.
    property string fillPath
    property color color: Style.ink
    property real strokeWidth: 2
    property real viewWidth: 24
    property real viewHeight: 24
    property bool roundJoins: true

    implicitWidth: 20
    implicitHeight: 20

    Shape {
        width: glyph.viewWidth
        height: glyph.viewHeight
        preferredRendererType: Shape.CurveRenderer
        transform: Scale {
            xScale: glyph.width / glyph.viewWidth
            yScale: glyph.height / glyph.viewHeight
        }

        ShapePath {
            strokeColor: glyph.path.length > 0 ? glyph.color : "transparent"
            strokeWidth: glyph.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: glyph.roundJoins ? ShapePath.RoundJoin : ShapePath.MiterJoin
            PathSvg { path: glyph.path.length > 0 ? glyph.path : "M0 0" }
        }

        ShapePath {
            strokeColor: "transparent"
            strokeWidth: 0
            fillColor: glyph.fillPath.length > 0 ? glyph.color : "transparent"
            PathSvg { path: glyph.fillPath.length > 0 ? glyph.fillPath : "M0 0" }
        }
    }
}
