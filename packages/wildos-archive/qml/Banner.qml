import QtQuick
import WildOS.UI as UI

// A note above the table (AppArchive.dc): 36px, radius 8, a 15px icon and
// 12.5px text. The read only note is tinted with the accent; `warning`
// notes take the warning colours.
Rectangle {
    id: banner

    property string text
    property string iconName: "info"
    property bool warning: false
    // Words that act, at the end of the note.
    property string action

    signal activated()

    implicitHeight: Math.max(36, label.implicitHeight + 16)
    radius: 8
    color: warning ? UI.Style.warningSoft : ArchiveStyle.bannerFill
    border.width: 1
    border.color: warning ? Qt.rgba(UI.Style.warning.r, UI.Style.warning.g, UI.Style.warning.b, 0.3) : ArchiveStyle.bannerEdge

    ArchiveIcon {
        id: icon
        anchors.left: parent.left
        anchors.leftMargin: 13
        anchors.verticalCenter: parent.verticalCenter
        name: banner.iconName
        size: 15
        color: banner.warning ? UI.Style.warning : ArchiveStyle.bannerIcon
    }
    UI.Label {
        id: label
        anchors.left: icon.right
        anchors.leftMargin: 10
        anchors.right: link.visible ? link.left : parent.right
        anchors.rightMargin: link.visible ? 8 : 12
        anchors.verticalCenter: parent.verticalCenter
        text: banner.text
        size: 12.5
        wrapMode: Text.Wrap
    }
    UI.LinkButton {
        id: link
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: banner.action.length > 0
        text: banner.action
        size: 13
        onClicked: banner.activated()
    }
}
