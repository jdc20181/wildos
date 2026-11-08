import QtQuick
import QtQuick.Shapes

// A small indeterminate ring for buttons and rows that are working.
Item {
    id: spinner

    property real size: 16
    property color color: Style.accent
    property bool running: visible

    implicitWidth: size
    implicitHeight: size

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        RotationAnimation on rotation {
            running: spinner.running
            from: 0
            to: 360
            duration: 900
            loops: Animation.Infinite
        }

        ShapePath {
            strokeColor: spinner.color
            strokeWidth: Math.max(1.5, spinner.size / 9)
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: spinner.size / 2
                centerY: spinner.size / 2
                radiusX: spinner.size / 2 - 1.5
                radiusY: spinner.size / 2 - 1.5
                startAngle: -90
                sweepAngle: 270
            }
        }
    }
}
