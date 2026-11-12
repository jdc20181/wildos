import QtQuick
import WildOS.UI as UI

// A small touchpad with dots for the fingers, looping through one gesture:
// the dots land, move and lift. help.wildos.io/using/touchpad-gestures/
// draws the same thing in CSS (src/styles/gestures.css in wildos-help), so
// the two must change together. With reduced motion the dots rest halfway
// through the movement, which still shows its direction.
Item {
    id: preview

    // swipe-up, swipe-down, swipe-sideways, pinch-in, pinch-out, page
    // (two fingers sideways), scroll, zoom, rotate, tap, drag.
    property string kind: "swipe-up"
    property int fingers: 3
    property bool running: visible && !!Window.window && Window.window.visible

    implicitWidth: 44
    implicitHeight: 32
    Accessible.ignored: true

    // One loop is 0 to 1; sideways gestures take two loops, left then right.
    property real t: 0.5
    readonly property bool twoWay: kind === "swipe-sideways" || kind === "page"
    readonly property real local: t % 1
    readonly property bool second: t >= 1

    NumberAnimation on t {
        running: preview.running && !UI.Style.reducedMotion
        from: 0
        to: preview.twoWay ? 2 : 1
        duration: preview.twoWay ? 3200 : 1600
        loops: Animation.Infinite
    }

    function ease(x) {
        return x < 0.5 ? 2 * x * x : 1 - Math.pow(-2 * x + 2, 2) / 2;
    }
    // How far along the movement the dots are, 0 to 1.
    readonly property real move: UI.Style.reducedMotion ? 0.5
        : local < 0.15 ? 0 : local > 0.65 ? 1 : ease((local - 0.15) / 0.5)
    readonly property real dotOpacity: UI.Style.reducedMotion ? 1
        : local < 0.1 ? local / 0.1 : local < 0.7 ? 1 : local < 0.8 ? 1 - (local - 0.7) / 0.1 : 0

    // Where finger i starts and ends, from the centre of the pad.
    function points(i, n) {
        const gap = 8;
        const x0 = -gap * (n - 1) / 2 + gap * i;
        const arc = (i === 0 || i === n - 1) ? 1.5 : 0;
        switch (kind) {
        case "swipe-up":
            return [x0, 6 + arc, x0, -6 + arc];
        case "swipe-down":
        case "scroll":
            return [x0, -6 + arc, x0, 6 + arc];
        case "swipe-sideways":
        case "page": {
            const dx = second ? 9 : -9;
            return [x0 - dx / 2, arc, x0 + dx / 2, arc];
        }
        case "pinch-in":
        case "pinch-out": {
            // A thumb low on the left and three fingers in an arc above.
            const spread = [[-11, 8], [-8, -5], [0, -8], [8, -5]];
            const p = spread[i % 4];
            const near = [p[0] * 0.3, p[1] * 0.3];
            return kind === "pinch-in" ? [p[0], p[1], near[0], near[1]] : [near[0], near[1], p[0], p[1]];
        }
        case "zoom": {
            const s = i === 0 ? -1 : 1;
            return [s * 4, -s * 3, s * 10, -s * 7];
        }
        case "drag":
            return [x0 - 6, 4 + arc, x0 + 6, -4 + arc];
        default:
            return [x0, arc, x0, arc];
        }
    }

    // Where finger i is now. Turning fingers go round a circle a quarter
    // turn clockwise; the others move straight.
    function at(i, n) {
        if (kind === "rotate") {
            const a = (i === 0 ? Math.PI : 0) + move * Math.PI / 2;
            return [7 * Math.cos(a), 7 * Math.sin(a)];
        }
        const p = points(i, n);
        return [p[0] + (p[2] - p[0]) * move, p[1] + (p[3] - p[1]) * move];
    }

    readonly property int dots: kind.startsWith("pinch") ? 4 : ["page", "scroll", "zoom", "rotate", "tap"].includes(kind) ? 2 : fingers

    Rectangle {
        anchors.fill: parent
        radius: 7
        color: UI.Style.surfaceTile
        border.width: 1
        border.color: UI.Style.inkAlpha(0.28, 0.24)

        Repeater {
            model: preview.dots

            Rectangle {
                required property int index
                readonly property var p: preview.at(index, preview.dots)
                readonly property real tap: preview.kind === "tap" && !UI.Style.reducedMotion
                    ? (preview.local > 0.2 && preview.local < 0.32 ? 1.35 : 1) : 1

                width: 6 * tap
                height: 6 * tap
                radius: width / 2
                color: UI.Style.accent
                opacity: preview.dotOpacity
                x: parent.width / 2 + p[0] - width / 2
                y: parent.height / 2 + p[1] - height / 2
            }
        }
    }
}
