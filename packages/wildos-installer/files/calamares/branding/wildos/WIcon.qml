/* Line icon on the design system's 24 pixel grid, drawn from SVG path data
 * so no icon theme or SVG image plugin is needed.
 */
import QtQuick

Canvas {
    id: icon

    property string path: ""
    property color stroke: "#b9cfff"
    property real strokeWidth: 1.9

    implicitWidth: 20
    implicitHeight: 20
    antialiasing: true

    onPathChanged: requestPaint()
    onStrokeChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();
        ctx.scale(width / 24, height / 24);
        ctx.strokeStyle = icon.stroke;
        ctx.lineWidth = icon.strokeWidth;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";
        ctx.path = icon.path;
        ctx.stroke();
    }
}
