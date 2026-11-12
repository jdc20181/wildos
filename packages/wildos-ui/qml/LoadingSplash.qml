import QtQuick
import QtQuick.Shapes
import QtQuick.Window

// The app loading splash shared by Mail, Write, Tables and Remote (board
// 12.5, LoSplash.dc and SplashDaily.dc): the app's icon and name, what it
// is opening, two short notes and a bar gliding in the app's colour.
// finish() fades it out into the window that takes over.
//
// Opening a document it waits a third of a second first, so a quick open
// never flashes. Launching an app it shows at once and stays long enough to
// read, as Office apps do: set showDelay to 0 and minimumTime.
Window {
    id: splash

    property string appName: ""
    property url iconSource
    // The app's colour, light end and deep end of its tile.
    property color accent: "#9d7bff"
    property color accentDeep: "#5631c9"
    property string status: ""
    property string noteStart: ""
    property string noteEnd: ""
    // LibreOffice gives no sign that it is ready, so Write and Tables take
    // the document's window gaining focus as the sign instead.
    property bool finishOnFocusLoss: false
    property int showDelay: 333
    property int minimumTime: 0
    property double shownAt: 0
    readonly property bool shown: visible

    signal finished()

    function finish() {
        if (!visible) {
            showTimer.stop();
            finished();
            return;
        }
        const left = minimumTime - (Date.now() - shownAt);
        if (left > 0) {
            holdTimer.interval = left;
            holdTimer.restart();
            return;
        }
        fade.start();
    }

    Timer {
        id: holdTimer
        onTriggered: fade.start()
    }

    width: 600
    height: 320
    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"
    title: appName
    visible: false

    Timer {
        id: showTimer
        interval: Math.max(1, splash.showDelay)
        running: true
        onTriggered: {
            splash.shownAt = Date.now();
            splash.visible = true;
            splash.requestActivate();
        }
    }

    onActiveChanged: {
        if (!active && visible && finishOnFocusLoss) {
            finish();
        }
    }

    NumberAnimation {
        id: fade
        target: card
        property: "opacity"
        to: 0
        duration: Style.reducedMotion ? 0 : 200
        onFinished: {
            splash.visible = false;
            splash.finished();
        }
    }

    Item {
        id: card
        anchors.fill: parent

        // The card is the same neutral surface as every window; the app's
        // colour shows only as a glow from two corners and in the bar. The
        // board's glows are ellipses, so fillTransform squashes each
        // circular gradient vertically.
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeWidth: -1
                fillGradient: LinearGradient {
                    // The board's linear-gradient(160deg) across 600 by 320.
                    x1: 213.5; y1: -77.8; x2: 386.5; y2: 397.8
                    GradientStop { position: 0; color: Style.dark ? "#202127" : "#fbfbfb" }
                    GradientStop { position: 0.6; color: Style.dark ? "#17181c" : "#f4f4f4" }
                    GradientStop { position: 1; color: Style.dark ? "#121317" : "#ededed" }
                }
                PathRectangle { width: 600; height: 320; radius: 16 }
            }
            ShapePath {
                strokeWidth: -1
                fillTransform: Qt.matrix4x4(1, 0, 0, 0,
                                            0, 260 / 420, 0, 0,
                                            0, 0, 1, 0,
                                            0, 0, 0, 1)
                fillGradient: RadialGradient {
                    centerX: 0; centerY: 0; centerRadius: 420
                    focalX: 0; focalY: 0
                    GradientStop { position: 0; color: Qt.alpha(splash.accent, 0.16) }
                    GradientStop { position: 0.72; color: Qt.alpha(splash.accent, 0) }
                }
                PathRectangle { width: 600; height: 320; radius: 16 }
            }
            ShapePath {
                strokeWidth: -1
                fillTransform: Qt.matrix4x4(1, 0, 0, 600,
                                            0, 220 / 360, 0, 320,
                                            0, 0, 1, 0,
                                            0, 0, 0, 1)
                fillGradient: RadialGradient {
                    centerX: 0; centerY: 0; centerRadius: 360
                    focalX: 0; focalY: 0
                    GradientStop { position: 0; color: Qt.alpha(splash.accent, 0.06) }
                    GradientStop { position: 0.72; color: Qt.alpha(splash.accent, 0) }
                }
                PathRectangle { width: 600; height: 320; radius: 16 }
            }
            ShapePath {
                strokeColor: Style.dark ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(0, 0, 0, 0.08)
                strokeWidth: 1
                fillColor: "transparent"
                PathRectangle { x: 0.5; y: 0.5; width: 599; height: 319; radius: 15.5 }
            }
        }

        Image {
            x: 36
            y: 34
            width: 72
            height: 72
            source: splash.iconSource
            sourceSize.width: 144
            sourceSize.height: 144
            smooth: true
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 40
            y: 44
            spacing: 8

            Image {
                width: 22
                height: 22
                anchors.verticalCenter: parent.verticalCenter
                source: "data/wildos-mark.svg"
                sourceSize.width: 44
                sourceSize.height: 44
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Wild OS"
                font.family: Style.fontFamily
                font.letterSpacing: Style.tracking(13)
                font.pixelSize: 13
                font.weight: Style.weightSemiBold
                color: Style.inkMuted
            }
        }

        Column {
            x: 40
            y: 136
            spacing: 6

            Text {
                text: splash.appName
                font.family: Style.fontFamily
                font.pixelSize: 36
                font.weight: Style.weightSemiBold
                font.letterSpacing: -0.72
                color: Style.ink
            }
            Text {
                width: 520
                elide: Text.ElideMiddle
                text: splash.status
                font.family: Style.fontFamily
                font.letterSpacing: Style.tracking(14)
                font.pixelSize: 14
                color: Style.inkMuted
            }
        }

        Item {
            x: 40
            y: 238
            width: 520
            height: 16

            Text {
                text: splash.noteStart
                font.family: Style.fontFamily
                font.letterSpacing: Style.tracking(12)
                font.pixelSize: 12
                color: Style.inkSubtle
            }
            Text {
                anchors.right: parent.right
                text: splash.noteEnd
                font.family: Style.fontFamily
                font.letterSpacing: Style.tracking(12)
                font.pixelSize: 12
                color: Style.inkSubtle
            }
        }

        Rectangle {
            x: 40
            y: 270
            width: 520
            height: 4
            radius: 2
            color: Style.dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.08)
            clip: true

            Rectangle {
                width: 156
                height: 4
                radius: 2
                x: Style.reducedMotion ? 182 : 0
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: splash.accentDeep }
                    GradientStop { position: 1; color: splash.accent }
                }

                SequentialAnimation on x {
                    running: !Style.reducedMotion && splash.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: 0; to: 364; duration: 1200; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: 364; to: 0; duration: 1200; easing.type: Easing.InOutQuad }
                }
            }
        }
    }
}
