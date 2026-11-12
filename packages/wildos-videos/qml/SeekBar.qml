import QtQuick
import WildOS.UI as UI

// The seek bar over the picture (AppVideos.dc): a 5px track, the accent up
// to a 12px white knob, and a dark tick where each chapter starts. Under
// the pointer a tip names the chapter there, "Chapter 2 · Sunrise", or the
// time when the video has no chapters. Dragging moves the knob; the video
// jumps when it is let go.
Item {
    id: bar

    property real position: 0
    property real duration: 0
    property var chapters: []
    readonly property bool dragging: mouse.pressed
    readonly property real shown: dragging ? dragValue : position
    property real dragValue: 0

    signal seekRequested(real seconds)

    implicitHeight: 18
    Accessible.role: Accessible.Slider
    Accessible.name: qsTr("Position in the video")

    function valueAt(x) {
        return bar.duration > 0 ? Math.max(0, Math.min(1, x / track.width)) * bar.duration : 0;
    }
    function tipText(seconds) {
        if (bar.chapters.length > 0) {
            let found = -1;
            for (let i = 0; i < bar.chapters.length; ++i) {
                if (bar.chapters[i].time <= seconds + 0.001) {
                    found = i;
                }
            }
            if (found >= 0) {
                return qsTr("Chapter %1 · %2").arg(bar.chapters[found].number).arg(bar.chapters[found].title);
            }
        }
        return VideosStyle.time(seconds);
    }

    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: mouse.containsMouse || mouse.pressed ? 6 : 5
        radius: height / 2
        color: VideosStyle.overPictureTrack

        Rectangle {
            width: bar.duration > 0 ? Math.max(0, Math.min(1, bar.shown / bar.duration)) * parent.width : 0
            height: parent.height
            radius: height / 2
            color: VideosStyle.overPictureAccent
        }
    }

    Repeater {
        model: bar.duration > 0 ? bar.chapters : []

        Rectangle {
            required property var modelData
            visible: modelData.time > 0.5 && modelData.time < bar.duration - 0.5
            x: Math.round(modelData.time / bar.duration * track.width) - 1
            y: 6
            width: 2
            height: 6
            color: VideosStyle.chapterTick
        }
    }

    Rectangle {
        id: knob
        visible: bar.duration > 0
        x: (bar.duration > 0 ? Math.max(0, Math.min(1, bar.shown / bar.duration)) * track.width : 0) - width / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 12
        height: 12
        radius: 6
        color: "#ffffff"
        scale: mouse.pressed ? 1.15 : 1
        Behavior on scale {
            NumberAnimation { duration: UI.Style.motionFast }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        anchors.topMargin: -6
        anchors.bottomMargin: -6
        hoverEnabled: true
        enabled: bar.duration > 0
        cursorShape: Qt.PointingHandCursor
        preventStealing: true
        onPressed: event => bar.dragValue = bar.valueAt(event.x)
        onPositionChanged: event => {
            if (pressed) {
                bar.dragValue = bar.valueAt(event.x);
            }
        }
        onReleased: event => bar.seekRequested(bar.valueAt(event.x))
    }

    Rectangle {
        id: tip
        readonly property real at: mouse.pressed ? bar.dragValue / Math.max(1, bar.duration) * track.width : mouse.mouseX
        visible: (mouse.containsMouse || mouse.pressed) && bar.duration > 0
        x: Math.round(Math.max(0, Math.min(bar.width - width, at - width / 2)))
        y: -46
        width: tipLabel.implicitWidth + 18
        height: tipLabel.implicitHeight + 10
        radius: UI.Style.radiusXs
        color: VideosStyle.tipGlass
        border.width: 1
        border.color: VideosStyle.tipEdge

        UI.Label {
            id: tipLabel
            anchors.centerIn: parent
            size: 12
            color: VideosStyle.overPicture
            text: bar.tipText(mouse.pressed ? bar.dragValue : bar.valueAt(mouse.mouseX))
        }
    }
}
