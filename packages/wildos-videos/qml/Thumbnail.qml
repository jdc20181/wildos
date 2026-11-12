import QtQuick
import QtQuick.Effects
import WildOS.Videos
import WildOS.UI as UI

// A still from a video with its length in the corner and, once it has
// been watched in part, a bar along the foot for how far (AppVideos.dc:
// 120 by 68, radius 6, the length on a dark chip). The still is made on
// this PC the first time it is shown.
Item {
    id: thumb

    property string path
    property real radius: 6
    // Overrides the remembered progress, for the video that is playing.
    property real liveProgress: -1
    property bool showLength: true

    property var info: ({})
    readonly property real progress: liveProgress >= 0 ? liveProgress : (Positions.revision, Positions.progress(path))
    readonly property bool hasImage: still.status === Image.Ready

    implicitWidth: 120
    implicitHeight: 68

    function refresh() {
        info = path.length > 0 ? Thumbnails.info(path) : {};
    }
    onPathChanged: refresh()
    Component.onCompleted: refresh()

    Connections {
        target: Thumbnails
        function onReady(path, info) {
            if (path === thumb.path) {
                thumb.info = info;
            }
        }
    }

    Item {
        id: content
        anchors.fill: parent
        visible: false
        layer.enabled: true

        Rectangle {
            anchors.fill: parent
            color: VideosStyle.picture
        }

        VideosIcon {
            anchors.centerIn: parent
            visible: !thumb.hasImage
            name: "film"
            size: Math.max(16, Math.round(thumb.height * 0.3))
            strokeWidth: 1.6
            color: Qt.rgba(1, 1, 1, 0.35)
        }

        Image {
            id: still
            anchors.fill: parent
            source: thumb.info.image ?? ""
            sourceSize.width: 512
            sourceSize.height: 288
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            smooth: true
            mipmap: true
            opacity: status === Image.Ready ? 1 : 0
            Behavior on opacity {
                NumberAnimation { duration: UI.Style.motionFast }
            }
        }

        Rectangle {
            visible: thumb.progress > 0
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: parent.width * Math.min(1, thumb.progress)
            height: 3
            color: UI.Style.accent
        }
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        visible: false
        radius: thumb.radius
        layer.enabled: true
        layer.smooth: true
    }

    MultiEffect {
        anchors.fill: parent
        source: content
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }

    Rectangle {
        visible: thumb.showLength && lengthLabel.text.length > 0
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 5
        anchors.bottomMargin: 5
        width: lengthLabel.implicitWidth + 10
        height: lengthLabel.implicitHeight + 2
        radius: 4
        color: VideosStyle.durationChip

        UI.Label {
            id: lengthLabel
            anchors.centerIn: parent
            size: 11
            color: VideosStyle.overPicture
            text: VideosStyle.length(thumb.info.duration ?? 0)
        }
    }
}
