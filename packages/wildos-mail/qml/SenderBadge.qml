import QtQuick
import WildOS.UI as UI

// A sender's initials in a coloured circle. The colour comes from the
// address, so the same person always looks the same.
Rectangle {
    id: badge

    property string initials
    property int colourIndex: 0
    property real size: 32

    readonly property var pairs: [
        ["#f59e8b", "#d9607e"], ["#6ec3ff", "#2f7ad8"], ["#8fd6a4", "#2f9a64"], ["#ffd27a", "#e59b2f"],
        ["#c5a3ff", "#7b55e0"], ["#ff9fc0", "#d4518a"], ["#9ad8d0", "#2c9a8f"], ["#a8b4cc", "#5d6a86"]
    ]
    readonly property var pair: pairs[Math.abs(colourIndex) % pairs.length]

    width: size
    height: size
    radius: size / 2
    gradient: Gradient {
        orientation: Gradient.Vertical
        GradientStop {
            position: 0
            color: badge.pair[0]
        }
        GradientStop {
            position: 1
            color: badge.pair[1]
        }
    }

    Text {
        anchors.centerIn: parent
        text: badge.initials
        color: "#ffffff"
        font.family: UI.Style.fontFamily
        font.pixelSize: Math.round(badge.size * 0.375)
        font.weight: Font.DemiBold
    }
}
