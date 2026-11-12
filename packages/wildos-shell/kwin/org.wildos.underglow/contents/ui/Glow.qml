import QtQuick
import QtQuick.Window
import org.wildos.shell as WildOS
import WildOS.UI as UI

// One taskbar's underglow: a strip window across the taskbar's top edge,
// reaching far enough above and below the line for the glow to fade out.
//
// The line lies on the taskbar's top two pixels, over its hairline, rather
// than just above it as the board draws it: above, it would cover the bottom
// two pixels of every maximised window.
Window {
    id: glow

    property var dock: null
    property bool ready: true
    property bool covered: false

    // KWin lets an internal window catch the pointer unless it carries this
    // (InternalWindow::hitTest); clicks go through to the taskbar and the
    // window above it.
    readonly property bool outputOnly: true
    readonly property bool _q_showWithoutActivating: true

    // Past 20px the glow and the spectrum's blurred copy are each under
    // one 8-bit step (underglowtest).
    readonly property int reach: 20
    readonly property int lineHeight: 2
    readonly property rect bar: dock && !dock.deleted ? dock.frameGeometry : Qt.rect(0, 0, 0, 0)

    readonly property bool rgb: WildOS.Underglow.colour === "rgb"
    readonly property color tint: WildOS.Underglow.colour === "custom" ? WildOS.Underglow.custom : UI.Style.accent
    // The board glows a single colour at 0.45 of itself and lays a soft
    // white at 0.25 under the spectrum.
    readonly property color glowColour: rgb ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(tint.r, tint.g, tint.b, 0.45)
    readonly property string effect: WildOS.Underglow.running
    readonly property bool flowing: visible && rgb && effect === "flowing"

    // The spectrum, each copy with its own gradient: one Gradient object
    // shared between many Rectangles crashed KWin's QML loader.
    component Spectrum: Rectangle {
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0 / 7; color: "#ff3d6e" }
            GradientStop { position: 1 / 7; color: "#ffb03d" }
            GradientStop { position: 2 / 7; color: "#f5ff3d" }
            GradientStop { position: 3 / 7; color: "#3dff7a" }
            GradientStop { position: 4 / 7; color: "#3dd5ff" }
            GradientStop { position: 5 / 7; color: "#5c7cff" }
            GradientStop { position: 6 / 7; color: "#c85cff" }
            GradientStop { position: 7 / 7; color: "#ff3d6e" }
        }
    }

    flags: Qt.FramelessWindowHint | Qt.BypassWindowManagerHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput
    color: "transparent"
    x: Math.round(bar.x)
    y: Math.round(bar.y) - reach
    width: Math.max(1, Math.round(bar.width))
    height: reach * 2 + lineHeight
    visible: WildOS.Underglow.enabled && !WildOS.TaskbarStyle.floating
        && dock !== null && !dock.deleted && !dock.hidden && !covered && bar.width > 0

    // Comes up once the desktop has faded in after signing in (main.qml),
    // not on its own over the start-up screen: KWin draws this window itself,
    // so nothing else holds it back.
    Item {
        anchors.fill: parent
        opacity: glow.ready ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: 450; easing.type: Easing.OutCubic }
        }

        Item {
            id: light
            anchors.fill: parent

            // The glow as one-pixel rows, each at the strength CSS's box-shadow
            // gives that distance from the line.
            Column {
                Repeater {
                    model: WildOS.Underglow.glowRows(glow.reach)
                    delegate: Rectangle {
                        required property real modelData
                        width: glow.width
                        height: 1
                        color: Qt.rgba(glow.glowColour.r, glow.glowColour.g, glow.glowColour.b, glow.glowColour.a * modelData)
                    }
                }
            }

            // Accent or custom: solid from 18% to 82%, fading to nothing at
            // both ends.
            Rectangle {
                visible: !glow.rgb
                y: glow.reach
                width: parent.width
                height: glow.lineHeight
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.rgba(glow.tint.r, glow.tint.g, glow.tint.b, 0) }
                    GradientStop { position: 0.18; color: glow.tint }
                    GradientStop { position: 0.82; color: glow.tint }
                    GradientStop { position: 1.0; color: Qt.rgba(glow.tint.r, glow.tint.g, glow.tint.b, 0) }
                }
            }

            // RGB: the spectrum one bar wide, twice over, sliding right by one
            // width per cycle so it wraps without a seam, as the board's
            // background-position animation does. Each copy carries the line and,
            // under it, the blurred copy of the spectrum as one-pixel rows at
            // half strength.
            Item {
                visible: glow.rgb
                width: parent.width
                height: parent.height
                clip: true

                Row {
                    id: spectrum

                    // Where the slide is, from the clock shared with the Warp
                    // wallpaper's W so the two stay in step. Held where it is
                    // when the flow pauses.
                    property real phase: WildOS.Underglow.flowPhase()

                    x: -glow.width * (1 - phase)

                    Repeater {
                        model: 2
                        delegate: Item {
                            width: glow.width
                            height: glow.height

                            Column {
                                Repeater {
                                    model: WildOS.Underglow.spectrumCopyRows(glow.reach)
                                    delegate: Spectrum {
                                        required property real modelData
                                        width: glow.width
                                        height: 1
                                        opacity: 0.5 * modelData
                                    }
                                }
                            }

                            Spectrum {
                                y: glow.reach
                                width: glow.width
                                height: glow.lineHeight
                            }
                        }
                    }
                }

                FrameAnimation {
                    running: glow.flowing
                    onTriggered: spectrum.phase = WildOS.Underglow.flowPhase()
                }
            }
        }
    }

    // Breathing: 1 to 0.35 and back every 3.2s with CSS's ease-in-out on
    // each half. It starts from full so turning it on does not blink.
    SequentialAnimation {
        running: glow.visible && !glow.rgb && glow.effect === "breathing"
        loops: Animation.Infinite
        onRunningChanged: {
            if (!running) {
                light.opacity = 1;
            }
        }

        NumberAnimation {
            target: light
            property: "opacity"
            to: 0.35
            duration: 1600
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.42, 0, 0.58, 1, 1, 1]
        }
        NumberAnimation {
            target: light
            property: "opacity"
            to: 1
            duration: 1600
            easing.type: Easing.BezierSpline
            easing.bezierCurve: [0.42, 0, 0.58, 1, 1, 1]
        }
    }
}
