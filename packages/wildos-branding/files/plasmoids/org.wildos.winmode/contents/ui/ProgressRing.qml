import QtQuick
import QtQuick.Shapes
import WildOS.UI as UI

// A ring that fills clockwise from the top with the accent (boards 9.11 to
// 9.13): on the taskbar mark while Windows mode sets up, and beside the
// step in progress in its panel.
Item {
    id: ring

    property real size: 14
    property real thickness: 2.4
    // 0 to 1
    property real value: 0

    implicitWidth: size
    implicitHeight: size
    Accessible.role: Accessible.ProgressBar

    readonly property real radius: (size - thickness) / 2

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: UI.Style.tone(0.22)
            strokeWidth: ring.thickness
            PathAngleArc {
                centerX: ring.size / 2
                centerY: ring.size / 2
                radiusX: ring.radius
                radiusY: ring.radius
                startAngle: 0
                sweepAngle: 360
            }
        }

        ShapePath {
            fillColor: "transparent"
            strokeColor: UI.Style.accent
            strokeWidth: ring.thickness
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: ring.size / 2
                centerY: ring.size / 2
                radiusX: ring.radius
                radiusY: ring.radius
                startAngle: -90
                sweepAngle: 360 * Math.max(0, Math.min(1, ring.value))
            }
        }
    }
}
