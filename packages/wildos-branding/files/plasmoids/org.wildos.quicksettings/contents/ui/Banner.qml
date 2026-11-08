import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A tinted note at the top of a page, like the design's highlighted
// notification card, with an optional action link.
Rectangle {
    id: banner

    property string text
    property string actionText
    property bool warning: false

    signal action()

    implicitHeight: content.implicitHeight + 20
    radius: UI.Style.radiusMd
    color: warning ? UI.Style.warningSoft : UI.Style.accentSoft
    border.width: 1
    border.color: warning ? Qt.rgba(UI.Style.warning.r, UI.Style.warning.g, UI.Style.warning.b, 0.30) : UI.Style.accentBorder

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        UI.Label {
            Layout.fillWidth: true
            text: banner.text
            size: 12
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        UI.LinkButton {
            visible: banner.actionText.length > 0
            text: banner.actionText
            size: 12
            onClicked: banner.action()
        }
    }
}
