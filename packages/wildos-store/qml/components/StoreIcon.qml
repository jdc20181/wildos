import QtQuick
import QtQuick.Shapes
import WildOS.UI as UI

// A line icon from the Store's own set in Metrics, or from WildOS.UI when
// the name is one of the system's.
Item {
    id: icon

    property string name
    property color color: UI.Style.iconInk
    property real size: 16
    property real strokeWidth: size <= 16 ? 2 : 1.8

    readonly property string path: Metrics.icons[name] || UI.Icons.paths[name] || ""

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
            strokeColor: icon.color
            strokeWidth: icon.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg {
                path: icon.path
            }
        }
    }
}
