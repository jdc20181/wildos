import QtQuick
import QtQuick.Shapes
import WildOS.TaskManager
import WildOS.UI as UI

// A usage graph as TaskManagerPerf.dc draws them: the last 60 seconds from
// left to right, newest at the right edge, a line over a soft fill of the
// same colour in a box edged in it. The large graph adds the 10 by 5 grid;
// the network's adds a second, dashed line for what is sent.
Item {
    id: graph

    property var values: []
    property var second: []
    property real maximum: 100
    property color colour: UI.Style.accent
    property real lineWidth: 1.6
    property real fillOpacity: 0.18
    property real edgeOpacity: 0.5
    property real radius: 6
    property bool grid: false
    // Samples in 60 seconds at the current update speed.
    property int capacity: Math.ceil(60000 / SystemStats.interval) + 1

    function points(list, closed) {
        const out = [];
        const n = list ? list.length : 0;
        if (n === 0 || width <= 2 || height <= 2) {
            return closed ? [Qt.point(0, height), Qt.point(width, height)] : [];
        }
        const step = (width - 2) / Math.max(1, capacity - 1);
        const start = width - 1 - (n - 1) * step;
        const top = 1;
        const span = height - 2;
        for (let i = 0; i < n; ++i) {
            const v = Math.max(0, Math.min(1, list[i] / Math.max(maximum, 0.0001)));
            out.push(Qt.point(start + i * step, top + span * (1 - v)));
        }
        if (closed) {
            out.push(Qt.point(start + (n - 1) * step, height - 1));
            out.push(Qt.point(start, height - 1));
        }
        return out;
    }

    clip: true

    Shape {
        anchors.fill: parent
        visible: graph.grid
        // Straight segments only, so the plain geometry renderer is the
        // cheap one; triangulating off the render thread keeps a page of
        // thirty graphs updating every second smooth.
        asynchronous: true

        ShapePath {
            strokeColor: UI.Style.divider
            strokeWidth: 1
            fillColor: "transparent"
            PathMultiline {
                paths: {
                    const lines = [];
                    for (let i = 1; i < 5; ++i) {
                        const y = Math.round(graph.height * i / 5) + 0.5;
                        lines.push([Qt.point(0, y), Qt.point(graph.width, y)]);
                    }
                    for (let i = 1; i < 10; ++i) {
                        const x = Math.round(graph.width * i / 10) + 0.5;
                        lines.push([Qt.point(x, 0), Qt.point(x, graph.height)]);
                    }
                    return lines;
                }
            }
        }
    }

    Shape {
        anchors.fill: parent
        asynchronous: true

        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillColor: Colours.alpha(graph.colour, graph.fillOpacity)
            PathPolyline {
                path: graph.points(graph.values, true)
            }
        }

        ShapePath {
            strokeWidth: graph.lineWidth
            strokeColor: graph.colour
            fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin
            capStyle: ShapePath.RoundCap
            PathPolyline {
                path: graph.points(graph.values, false)
            }
        }

        ShapePath {
            strokeWidth: graph.second.length > 0 ? graph.lineWidth : 0
            strokeColor: graph.second.length > 0 ? graph.colour : "transparent"
            strokeStyle: ShapePath.DashLine
            dashPattern: [3, 2]
            fillColor: "transparent"
            PathPolyline {
                path: graph.points(graph.second, false)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: graph.radius
        color: "transparent"
        border.width: 1
        border.color: Colours.alpha(graph.colour, graph.edgeOpacity)
    }
}
