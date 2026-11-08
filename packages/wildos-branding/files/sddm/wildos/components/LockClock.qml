import QtQuick
import QtQuick.Effects
import WildOS.UI as UI

// Day-first date over a large 24-hour time, as on the lock screen. The time
// box is exactly 136px tall like the design's line-height: 1, so the glyphs
// are shifted up by half the font's extra leading.
Item {
    id: clock

    property date now: new Date()

    implicitWidth: Math.max(dateLabel.implicitWidth, timeLabel.implicitWidth)
    implicitHeight: dateLabel.height + 6 + timeBox.height

    Timer {
        // Ticking every second keeps the minute change on time without
        // having to align a longer timer to the wall clock.
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: clock.now = new Date()
    }

    FontMetrics {
        id: timeMetrics
        font: timeLabel.font
    }

    Item {
        id: content
        anchors.fill: parent

        Text {
            id: dateLabel
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.locale().toString(clock.now, "dddd d MMMM")
            font.family: UI.Style.fontFamily
            font.pixelSize: UI.Style.px(22)
            font.weight: Font.Normal
            color: UI.Style.inkOnWallpaper
        }

        Item {
            id: timeBox
            anchors.top: dateLabel.bottom
            anchors.topMargin: 6
            anchors.horizontalCenter: parent.horizontalCenter
            width: timeLabel.implicitWidth
            height: 136

            Text {
                id: timeLabel
                anchors.horizontalCenter: parent.horizontalCenter
                y: (136 - timeMetrics.height) / 2
                text: Qt.formatTime(clock.now, "HH:mm")
                font.family: "Inter Display"
                renderType: Text.CurveRendering
                font.pixelSize: 136
                font.weight: Font.ExtraLight
                font.letterSpacing: -0.04 * 136
                color: UI.Style.inkOnWallpaper
            }
        }

        layer.enabled: GraphicsInfo.api !== GraphicsInfo.Software
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: UI.Style.textShadowOnWallpaper
            shadowBlur: 1.0
            blurMax: 48
            shadowVerticalOffset: 2
            shadowHorizontalOffset: 0
        }
    }
}
