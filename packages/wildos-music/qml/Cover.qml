import QtQuick
import QtQuick.Effects
import WildOS.UI as UI

// Album art cut to rounded corners, or a circle for an artist. With no art
// the tile is the neutral surface with a music note, never an invented
// colour.
Item {
    id: cover

    property url source
    property real radius: 12
    property bool round: false
    property string placeholder: "song"

    readonly property bool hasArt: art.status === Image.Ready
    readonly property real cornerRadius: round ? width / 2 : radius

    implicitWidth: 200
    implicitHeight: 200

    Rectangle {
        anchors.fill: parent
        visible: !cover.hasArt
        radius: cover.cornerRadius
        color: MusicStyle.noCover

        MusicIcon {
            anchors.centerIn: parent
            name: cover.placeholder
            size: Math.max(14, Math.round(cover.width * 0.32))
            strokeWidth: cover.width >= 120 ? 1.4 : cover.width >= 48 ? 1.6 : 2
            color: UI.Style.inkSubtle
        }
    }

    Image {
        id: art
        anchors.fill: parent
        visible: false
        source: cover.source
        sourceSize.width: Math.ceil(cover.width * 2)
        sourceSize.height: Math.ceil(cover.height * 2)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        smooth: true
        mipmap: true
        cache: true
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        visible: false
        radius: cover.cornerRadius
        layer.enabled: true
        layer.smooth: true
    }

    MultiEffect {
        anchors.fill: parent
        visible: cover.hasArt
        source: art
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }
}
