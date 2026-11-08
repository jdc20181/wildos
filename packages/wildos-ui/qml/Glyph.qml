import QtQuick
import QtQuick.Shapes

// A line icon from raw SVG path data, for shapes that are not in Icons.qml
// (battery levels, signal bars drawn from live values). The plasmoids'
// Glyph.qml, with an optional filled part and an optional dashed part (the
// screenshot toolbar's selection shapes, stroke-dasharray 3 2).
Item {
    id: glyph

    property string path
    property string fillPath
    property string dashedPath
    property color color: Style.iconInk
    property real strokeWidth: 2
    property real viewWidth: 24
    property real viewHeight: 24

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
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: glyph.path.length > 0 ? glyph.path : "M0 0" }
        }

        ShapePath {
            strokeColor: glyph.dashedPath.length > 0 ? glyph.color : "transparent"
            strokeWidth: glyph.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            joinStyle: ShapePath.RoundJoin
            strokeStyle: ShapePath.DashLine
            // In units of the stroke width: 3px on, 2px off.
            dashPattern: [3 / glyph.strokeWidth, 2 / glyph.strokeWidth]
            PathSvg { path: glyph.dashedPath.length > 0 ? glyph.dashedPath : "M0 0" }
        }

        ShapePath {
            strokeColor: "transparent"
            strokeWidth: 0
            fillColor: glyph.fillPath.length > 0 ? glyph.color : "transparent"
            PathSvg { path: glyph.fillPath.length > 0 ? glyph.fillPath : "M0 0" }
        }
    }
}
