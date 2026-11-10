import QtQuick
import QtQuick.Shapes
import WildOS.UI as UI

// Processor use over the last minute, as on board 4.14: a line in the
// accent over a soft fill, oldest on the left, with a dot on the latest
// reading. points: [{x: 0 to 1, y: 0 to 100}].
Item {
    id: spark

    property var points: []
    // The line keeps clear of the top so a busy minute's peak and the end
    // dot are not clipped.
    readonly property real headroom: 4
    readonly property var line: points.map(p => Qt.point(p.x * width, headroom + (height - headroom) * (1 - p.y / 100)))

    implicitHeight: 34

    Shape {
        anchors.fill: parent
        visible: spark.line.length > 1
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: "transparent"
            fillColor: UI.Style.accentChip
            PathPolyline {
                path: spark.line.length > 1
                    ? [Qt.point(spark.line[0].x, spark.height)].concat(spark.line, [Qt.point(spark.line[spark.line.length - 1].x, spark.height)])
                    : []
            }
        }

        ShapePath {
            strokeColor: UI.Style.accent
            strokeWidth: 1.6
            fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin
            capStyle: ShapePath.RoundCap
            PathPolyline {
                path: spark.line
            }
        }
    }

    Rectangle {
        visible: spark.line.length > 0
        readonly property point last: spark.line.length > 0 ? spark.line[spark.line.length - 1] : Qt.point(0, 0)
        x: Math.min(spark.width - width, last.x - width / 2)
        y: last.y - height / 2
        width: 6
        height: 6
        radius: 3
        color: UI.Style.accent
    }
}
