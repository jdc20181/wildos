import QtQuick
import QtQuick.Shapes
import WildOS.UI as UI

// The Store's progress ring from StoreStates.dc.html, drawn on the board's
// 28px grid (radius 11.5, 2.4 stroke) at any size. "dashed" is waiting in
// the queue, "progress" fills with `progress`, "spin" is an open arc that
// turns while the amount of work is unknown, "paused" holds the filled
// part in muted ink. `centre` puts the stop square or the play triangle in
// the middle.
Item {
    id: ring

    property string mode: "progress"
    property real progress: 0
    property string centre: "none"
    property real size: 28
    property color color: UI.Style.accent
    property color trackColor: Metrics.ringTrack

    readonly property real k: size / 28
    readonly property bool turning: (mode === "spin" || mode === "dashed") && visible && opacity > 0
    // The board's open arc is 20.2 of the 72.3 circumference.
    readonly property real spinSweep: 100

    property real shown: progress
    Behavior on shown {
        enabled: !UI.Style.reducedMotion
        NumberAnimation { duration: 600; easing.type: Easing.OutCubic }
    }

    implicitWidth: size
    implicitHeight: size

    // A filling ring always starts from the top, wherever the arc had
    // turned to.
    onTurningChanged: if (!turning) turner.rotation = 0

    Item {
        id: turner
        anchors.fill: parent

        // The queue's dotted ring: the board's 2.2 on, 3.2 off around the
        // 72.3 circumference, as thirteen short dashes.
        Repeater {
            model: ring.mode === "dashed" ? 13 : 0

            Rectangle {
                required property int index
                readonly property real angle: index * 2 * Math.PI / 13
                width: 2.4 * ring.k
                height: 2.2 * ring.k
                x: ring.size / 2 + 11.5 * ring.k * Math.sin(angle) - width / 2
                y: ring.size / 2 - 11.5 * ring.k * Math.cos(angle) - height / 2
                rotation: index * 360 / 13 + 90
                antialiasing: true
                color: ring.trackColor
            }
        }

        RotationAnimator on rotation {
            running: ring.turning
            loops: Animation.Infinite
            from: 0
            to: 360
            // The queue ring drifts; the working arc turns about once a
            // second. Reduced motion slows both to a crawl rather than
            // freezing them, so the button still reads as busy.
            duration: (ring.mode === "dashed" ? 9000 : 1100) * (UI.Style.reducedMotion ? 4 : 1)
        }

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: ring.mode === "dashed" ? "transparent" : ring.trackColor
                strokeWidth: 2.4 * ring.k
                fillColor: "transparent"
                PathAngleArc {
                    centerX: ring.size / 2
                    centerY: ring.size / 2
                    radiusX: 11.5 * ring.k
                    radiusY: 11.5 * ring.k
                    startAngle: 0
                    sweepAngle: 360
                }
            }

            ShapePath {
                strokeColor: ring.mode === "dashed" ? "transparent" : ring.mode === "paused" ? Metrics.inkSecondary : ring.color
                strokeWidth: 2.4 * ring.k
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathAngleArc {
                    centerX: ring.size / 2
                    centerY: ring.size / 2
                    radiusX: 11.5 * ring.k
                    radiusY: 11.5 * ring.k
                    startAngle: -90
                    sweepAngle: ring.mode === "spin" ? ring.spinSweep
                        : ring.mode === "dashed" ? 0
                        : Math.max(0.5, Math.min(100, ring.shown)) * 3.6
                }
            }
        }
    }

    Rectangle {
        visible: ring.centre === "stop"
        anchors.centerIn: parent
        width: 7 * ring.k
        height: 7 * ring.k
        radius: 1.5 * ring.k
        color: ring.color
    }

    Shape {
        visible: ring.centre === "play"
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: "transparent"
            fillColor: Metrics.inkSecondary
            PathSvg {
                path: "M" + 11.5 * ring.k + " " + 9.5 * ring.k + "l" + 7 * ring.k + " " + 4.5 * ring.k + "l" + -7 * ring.k + " " + 4.5 * ring.k + "z"
            }
        }
    }
}
