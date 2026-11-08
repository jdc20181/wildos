import QtQuick
import WildOS.UI as UI

// The band at the foot of Start, with a hairline along its top. Its lower
// corners follow the 22px glass radius, one pixel inside the border.
Rectangle {
    color: UI.Style.surfaceBand
    topLeftRadius: 0
    topRightRadius: 0
    bottomLeftRadius: Style.radiusStart - 1
    bottomRightRadius: Style.radiusStart - 1

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: UI.Style.tone(0.08)
    }
}
