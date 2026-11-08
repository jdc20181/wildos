import QtQuick
import QtQuick.Shapes

// One of the line icons in Icons.qml at any size, stroke and colour. The
// design's stroke is 1.8, or 2 at 16px and below.
Item {
    id: icon

    property string name
    property color color: Style.iconInk
    property real size: 16
    property real strokeWidth: size <= 16 ? 2 : 1.8

    implicitWidth: size
    implicitHeight: size

    Shape {
        width: 24
        height: 24
        preferredRendererType: Shape.CurveRenderer
        transform: Scale {
            xScale: icon.size / 24
            yScale: icon.size / 24
        }

        ShapePath {
            readonly property bool solid: Icons.filled[icon.name] === true
            strokeColor: solid ? "transparent" : icon.color
            strokeWidth: icon.strokeWidth
            fillColor: solid ? icon.color : "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg {
                path: Icons.paths[icon.name] || ""
            }
        }
    }
}
