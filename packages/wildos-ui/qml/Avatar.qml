import QtQuick
import QtQuick.Effects

// The account picture, or the Wild OS default (a square crop of the
// wallpaper, /usr/share/wildos/avatar-default.png) when the account has
// none. Either is scaled to cover the square and cropped to the circle, as
// Settings, Start, the lock screen and the sign-in screen draw it; this is
// wildos-settings Avatar without its System binding, so pass the account's
// picture as `source`. If even the default is missing the design's
// silhouette on the dusk disc shows instead. `ring` adds the lock screen's
// 4px glass edge.
Item {
    id: avatar

    property real size: 60
    property url source
    property url defaultPicture: "file:///usr/share/wildos/avatar-default.png"
    property bool ring: false
    // The soft drop shadow under the lock and sign-in screens' avatar
    // (Lock.dc: 0 16px 40px rgba(6,10,28,0.35)).
    property bool shadow: false

    readonly property bool hasPicture: own.status === Image.Ready || fallback.status === Image.Ready

    implicitWidth: size
    implicitHeight: size

    Rectangle {
        id: shadowShape
        anchors.fill: parent
        visible: false
        radius: width / 2
        color: Qt.rgba(6 / 255, 10 / 255, 28 / 255, 0.35)
        layer.enabled: avatar.shadow
    }

    MultiEffect {
        visible: avatar.shadow
        anchors.fill: parent
        anchors.topMargin: 16
        anchors.bottomMargin: -16
        source: avatar.shadow ? shadowShape : null
        blurEnabled: true
        blur: 1.0
        blurMax: 40
        autoPaddingEnabled: true
    }

    Rectangle {
        anchors.fill: parent
        visible: !avatar.hasPicture
        radius: width / 2
        clip: true
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0; color: "#3a4a78" }
            GradientStop { position: 1; color: "#232d4f" }
        }

        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            name: "avatar"
            size: avatar.size * 0.62
            color: Qt.rgba(232 / 255, 236 / 255, 248 / 255, 0.82)
        }
    }

    Item {
        id: picture
        anchors.fill: parent
        visible: false

        Image {
            id: own
            anchors.fill: parent
            source: avatar.source
            sourceSize.width: avatar.size * 2
            sourceSize.height: avatar.size * 2
            fillMode: Image.PreserveAspectCrop
            smooth: true
            mipmap: true
        }

        Image {
            id: fallback
            anchors.fill: parent
            // Not "visible ? ...": the container is hidden (the effect draws
            // it), so visible always reads false in here.
            visible: own.status !== Image.Ready
            source: own.status !== Image.Ready ? avatar.defaultPicture : ""
            sourceSize.width: avatar.size * 2
            sourceSize.height: avatar.size * 2
            fillMode: Image.PreserveAspectCrop
            smooth: true
            mipmap: true
        }
    }

    Rectangle {
        id: disc
        anchors.fill: parent
        visible: false
        radius: width / 2
        layer.enabled: true
        layer.smooth: true
    }

    MultiEffect {
        anchors.fill: parent
        visible: avatar.hasPicture
        source: picture
        maskEnabled: true
        maskSource: disc
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }

    Rectangle {
        visible: avatar.ring
        anchors.fill: parent
        anchors.margins: -4
        radius: width / 2
        color: "transparent"
        border.width: 4
        border.color: Qt.rgba(1, 1, 1, 0.18)
    }
}
