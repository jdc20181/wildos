import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// The column names over a track table: #, Title, Artist, the heart's
// column and a clock over the lengths, 12px subtle on a hairline. It sits
// 14px further in than the rows, as on the board.
Item {
    id: header

    property bool showAlbum: false
    property real artistWidth: 200

    implicitHeight: 30

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: UI.Style.borderSubtle
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 14

        UI.Label {
            Layout.preferredWidth: 22
            horizontalAlignment: Text.AlignHCenter
            text: "#"
            size: 12
            color: UI.Style.inkSubtle
        }
        UI.Label {
            Layout.fillWidth: true
            text: qsTr("Title")
            size: 12
            color: UI.Style.inkSubtle
        }
        UI.Label {
            Layout.preferredWidth: header.artistWidth
            text: qsTr("Artist")
            size: 12
            color: UI.Style.inkSubtle
        }
        UI.Label {
            visible: header.showAlbum
            Layout.preferredWidth: header.artistWidth
            text: qsTr("Album")
            size: 12
            color: UI.Style.inkSubtle
        }
        Item {
            Layout.preferredWidth: 15
        }
        Item {
            Layout.preferredWidth: 50
            Layout.fillHeight: true

            MusicIcon {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                name: "clock"
                size: 13
                color: UI.Style.inkSubtle
                Accessible.name: qsTr("Length")
            }
        }
    }
}
