import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A section title with an optional link at the right, baselines aligned.
Item {
    id: header

    property string title
    property string linkText
    property int fontSize: 17

    signal linkClicked()

    implicitHeight: heading.implicitHeight
    Layout.fillWidth: true

    UI.Label {
        id: heading
        text: header.title
        size: header.fontSize
        font.weight: Font.DemiBold
        width: parent.width - (link.visible ? link.width + 16 : 0)
        Accessible.role: Accessible.Heading
    }

    UI.LinkButton {
        id: link
        visible: header.linkText.length > 0
        anchors.right: parent.right
        anchors.rightMargin: -4
        y: heading.baselineOffset - contentItem.y - contentItem.baselineOffset
        text: header.linkText
        onClicked: header.linkClicked()
    }
}
