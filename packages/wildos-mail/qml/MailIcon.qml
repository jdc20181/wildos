import QtQuick
import QtQuick.Shapes
import WildOS.UI as UI

// One of Mail's line icons (MailIcons), or a system one by the same name.
Item {
    id: icon

    property string name
    property color color: UI.Style.iconInk
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
            strokeColor: icon.color
            strokeWidth: icon.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg {
                path: MailIcons.paths[icon.name] || UI.Icons.paths[icon.name] || ""
            }
        }
    }
}
