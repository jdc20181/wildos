import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A section's 21px heading with an optional link at the right, over the
// boards' hairline.
Item {
    id: header

    property string title
    property string linkText

    signal linkClicked()

    implicitHeight: heading.implicitHeight + 11
    Layout.fillWidth: true

    UI.Label {
        id: heading
        text: header.title
        size: 21
        font.weight: Font.Bold
        font.letterSpacing: -0.21
        width: parent.width - (link.visible ? link.width + 16 : 0)
        Accessible.role: Accessible.Heading
    }

    StoreLink {
        id: link
        visible: header.linkText.length > 0
        anchors.right: parent.right
        anchors.rightMargin: -4
        y: heading.baselineOffset - baselineOffset
        text: header.linkText
        onClicked: header.linkClicked()
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Metrics.divider
    }
}
