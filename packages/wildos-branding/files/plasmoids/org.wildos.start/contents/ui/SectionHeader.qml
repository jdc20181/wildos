import QtQuick
import WildOS.UI as UI

// A section title with its small pill button on the right ("All apps", "More").
Item {
    id: header

    property string title
    property string buttonText
    property bool backward: false
    property alias button: button

    signal activated()

    implicitHeight: Math.max(28, titleLabel.implicitHeight)

    UI.Label {
        id: titleLabel
        x: 6
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(implicitWidth, button.x - 12)
        size: 14
        font.weight: Font.DemiBold
        text: header.title
        Accessible.role: Accessible.Heading
    }

    WButton {
        id: button
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        width: content.implicitWidth + 20
        height: Math.max(28, content.implicitHeight + 10)
        radius: 7
        baseColor: UI.Style.tone(0.07)
        accessibleName: header.buttonText
        visible: header.buttonText.length > 0
        onClicked: header.activated()

        Row {
            id: content
            anchors.centerIn: parent
            spacing: 6
            layoutDirection: header.backward ? Qt.RightToLeft : Qt.LeftToRight

            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                size: 12
                text: header.buttonText
            }

            UI.Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: header.backward ? "chevron-left" : "chevron"
                size: 11
                strokeWidth: 2.4
            }
        }
    }
}
