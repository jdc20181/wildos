import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// A hover label over the picture, in the board's dark glass ("Chapter 2 ·
// Sunrise", AppVideos.dc): 12px white text, radius 6, above the control,
// since the controls sit at the foot of the picture.
T.ToolTip {
    id: control

    delay: 600
    timeout: 6000
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: -height - 8
    margins: 6
    leftPadding: 9
    rightPadding: 9
    topPadding: 5
    bottomPadding: 5
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    closePolicy: T.Popup.NoAutoClose

    contentItem: UI.Label {
        text: control.text
        size: 12
        color: VideosStyle.overPicture
    }

    background: Rectangle {
        radius: UI.Style.radiusXs
        color: VideosStyle.tipGlass
        border.width: 1
        border.color: VideosStyle.tipEdge
    }
}
