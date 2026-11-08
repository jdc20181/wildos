import QtQuick
import QtQuick.Effects
import WildOS.UI as UI

// One calm line under the password pill: a failed attempt, a message from
// PAM, or the Caps Lock hint. Plain text with a soft shadow, never red.
Item {
    id: notice

    property bool shown: false
    property string text
    property string iconName
    property bool muted: false

    width: parent ? parent.width : 0
    height: shown ? noticeRow.implicitHeight : 0
    visible: shown
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 200 } }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: text

    function pulse() {
        pulseAnimation.restart();
    }

    SequentialAnimation {
        id: pulseAnimation
        NumberAnimation { target: noticeRow; property: "opacity"; to: 0.35; duration: 120 }
        NumberAnimation { target: noticeRow; property: "opacity"; to: 1; duration: 220 }
    }

    Row {
        id: noticeRow
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6

        layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: UI.Style.textShadowOnWallpaper
            shadowBlur: 0.5
            blurMax: 24
            shadowVerticalOffset: 1
            shadowHorizontalOffset: 0
        }

        UI.Icon {
            visible: notice.iconName.length > 0
            anchors.verticalCenter: parent.verticalCenter
            name: notice.iconName
            size: 14
            color: notice.muted ? UI.Style.inkOnWallpaperMuted : UI.Style.inkOnWallpaper
        }

        UI.Label {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, notice.width - 20)
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: notice.text
            size: notice.muted ? 12 : 13
            color: notice.muted ? UI.Style.inkOnWallpaperMuted : UI.Style.inkOnWallpaper
        }
    }
}
