import QtQuick
import QtQuick.Shapes
import WildOS.UI as UI

// The low battery mark from SystemPrompts.dc: an outline with one short
// bar of charge left, amber on a soft amber disc.
Rectangle {
    width: 40
    height: 40
    radius: 20
    color: UI.Style.warningSoft

    Item {
        anchors.centerIn: parent
        width: 24
        height: 24
        scale: 20 / 24

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: UI.Style.warning
                strokeWidth: 2
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg {
                    path: "M4 7h13a2 2 0 0 1 2 2v6a2 2 0 0 1-2 2h-13a2 2 0 0 1-2-2v-6a2 2 0 0 1 2-2zM22 11v2"
                }
            }
        }

        Rectangle {
            x: 4
            y: 9
            width: 3
            height: 6
            radius: 1
            color: UI.Style.warning
        }
    }
}
