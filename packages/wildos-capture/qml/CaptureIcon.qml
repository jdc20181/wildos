import QtQuick
import QtQuick.Shapes
import WildOS.UI as UI

// One of Capture's line icons (CaptureIcons), or a system one by the same
// name.
Item {
    id: icon

    property string name
    property color color: UI.Style.iconInk
    property real size: 16
    property real strokeWidth: size <= 16 ? 2 : 1.8
    readonly property bool own: CaptureIcons.paths[name] !== undefined

    implicitWidth: size
    implicitHeight: size

    Shape {
        visible: icon.own
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
                path: icon.own ? CaptureIcons.paths[icon.name] : "M0 0"
            }
        }
    }

    UI.Icon {
        visible: !icon.own
        anchors.fill: parent
        name: icon.own ? "" : icon.name
        size: icon.size
        strokeWidth: icon.strokeWidth
        color: icon.color
    }
}
