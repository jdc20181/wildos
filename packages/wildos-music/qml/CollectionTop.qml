import QtQuick
import QtQuick.Layouts
import WildOS.Music
import WildOS.UI as UI

// The top of an album, artist or playlist page (AppMusic.dc): the 200px
// cover, a small capitals label, the 42px title, a line of facts, then
// Play, Shuffle, the heart and More.
Item {
    id: head

    property url cover
    property bool round: false
    property string placeholder: "album"
    property string label
    property string title
    property string facts
    property var paths: []
    property bool canPlay: paths.length > 0

    signal playClicked()
    signal shuffleClicked()
    signal moreClicked(Item button)

    implicitHeight: content.implicitHeight + 28 + 22

    RowLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 28
        spacing: 28

        Cover {
            Layout.preferredWidth: 200
            Layout.preferredHeight: 200
            Layout.alignment: Qt.AlignBottom
            source: head.cover
            round: head.round
            radius: 12
            placeholder: head.placeholder
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignBottom
            spacing: 8

            UI.Label {
                Layout.fillWidth: true
                text: head.label.toUpperCase()
                size: 12
                font.weight: UI.Style.weightSemiBold
                font.letterSpacing: UI.Style.px(12) * 0.06
                color: UI.Style.inkSubtle
            }
            UI.Label {
                Layout.fillWidth: true
                text: head.title
                size: 42
                font.weight: UI.Style.weightBold
                font.letterSpacing: -0.01 * UI.Style.px(42)
                Accessible.role: Accessible.Heading
            }
            UI.Label {
                Layout.fillWidth: true
                text: head.facts
                size: 14
                color: UI.Style.inkMuted
            }
            RowLayout {
                Layout.topMargin: 10
                spacing: 8

                PageButton {
                    primary: true
                    iconName: "play"
                    text: qsTr("Play")
                    enabled: head.canPlay
                    onClicked: head.playClicked()
                }
                PageButton {
                    iconName: "shuffle"
                    text: qsTr("Shuffle")
                    enabled: head.canPlay
                    onClicked: head.shuffleClicked()
                }
                HeartButton {
                    side: 34
                    iconSize: 18
                    paths: head.paths
                    restColor: UI.Style.iconInk
                }
                ToolButton {
                    id: more
                    iconName: "dots"
                    iconSize: 18
                    tip: qsTr("More")
                    enabled: head.canPlay
                    onClicked: head.moreClicked(more)
                }
            }
        }
    }
}
