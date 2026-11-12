import QtQuick
import QtQuick.Templates as T
import QtQuick.Layouts
import WildOS.Music
import WildOS.UI as UI

// The column the queue and lyrics buttons open at the right of the page:
// the songs still to play, which can be jumped to or taken out, or the
// playing song's own lyrics.
Rectangle {
    id: panel

    property string mode
    property var app

    signal closeRequested()

    color: MusicStyle.sideShade

    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: UI.Style.borderSubtle
    }

    RowLayout {
        id: heading
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 20
        anchors.rightMargin: 12
        anchors.topMargin: 18
        spacing: 8

        UI.Label {
            Layout.fillWidth: true
            text: panel.mode === "lyrics" ? qsTr("Lyrics") : qsTr("Queue")
            size: 15
            font.weight: UI.Style.weightSemiBold
            Accessible.role: Accessible.Heading
        }
        UI.Button {
            visible: panel.mode === "queue" && Player.index + 1 < Player.queue.length
            kind: "text"
            size: "small"
            text: qsTr("Clear")
            onClicked: Player.clearUpNext()
        }
        ToolButton {
            iconName: "x"
            side: 30
            tip: qsTr("Close")
            onClicked: panel.closeRequested()
        }
    }

    Flickable {
        id: lyrics
        visible: panel.mode === "lyrics"
        anchors.top: heading.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        contentHeight: lyricsText.implicitHeight + 32
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: UI.ScrollBar {}

        UI.Label {
            id: lyricsText
            x: 20
            width: lyrics.width - 40
            text: {
                const path = Player.currentPath;
                return panel.mode === "lyrics" && path.length > 0 ? Library.lyrics(path) : "";
            }
            size: 15
            lineHeight: 1.45
            wrapMode: Text.Wrap
            elide: Text.ElideNone
            color: UI.Style.ink
        }
        UI.Label {
            x: 20
            visible: lyricsText.text.length === 0
            width: lyrics.width - 40
            text: qsTr("This song has no lyrics in its file.")
            size: 13
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
        }
    }

    ListView {
        id: queue
        visible: panel.mode === "queue"
        anchors.top: heading.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: UI.ScrollBar {}
        bottomMargin: 12
        model: {
            Library.revision;
            return panel.mode === "queue" ? Library.tracks(Player.queue) : [];
        }

        delegate: Item {
            id: entry
            required property var modelData
            required property int index
            readonly property bool isCurrent: index === Player.index
            readonly property bool played: index < Player.index

            width: queue.width
            height: (index === Player.index || index === Player.index + 1 ? 30 : 0) + 52

            UI.Label {
                visible: entry.index === Player.index || entry.index === Player.index + 1
                x: 20
                height: 30
                text: entry.isCurrent ? qsTr("Now playing") : qsTr("Up next")
                size: 12
                font.weight: UI.Style.weightSemiBold
                color: UI.Style.inkSubtle
            }

            Item {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                height: 52
                opacity: entry.played ? 0.55 : 1

                Rectangle {
                    anchors.fill: parent
                    radius: UI.Style.radiusXs
                    color: entry.isCurrent ? UI.Style.surfaceSelected : rowMouse.containsMouse ? UI.Style.surfaceHover : "transparent"
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function (event) {
                        if (event.button === Qt.RightButton) {
                            panel.app.trackMenu(entry.modelData, "", rowMouse, event.x, event.y);
                        }
                    }
                    onDoubleClicked: Player.jump(entry.index)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 6
                    spacing: 10

                    Cover {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        radius: 4
                        source: entry.modelData.cover

                        Bars {
                            anchors.centerIn: parent
                            visible: entry.isCurrent && parent.hasArt
                            color: "#ffffff"
                            running: Player.playing
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        UI.Label {
                            Layout.fillWidth: true
                            text: entry.modelData.title
                            size: 13
                            color: entry.isCurrent ? MusicStyle.playingInk : UI.Style.ink
                            font.weight: entry.isCurrent ? UI.Style.weightSemiBold : UI.Style.weightRegular
                        }
                        UI.Label {
                            Layout.fillWidth: true
                            text: entry.modelData.artist
                            size: 12
                            color: UI.Style.inkMuted
                        }
                    }
                    ToolButton {
                        visible: !entry.isCurrent && (rowMouse.containsMouse || hovered)
                        side: 28
                        iconSize: 14
                        iconName: "x"
                        tip: qsTr("Take out of the queue")
                        onClicked: Player.removeFromQueue(entry.index)
                    }
                }
            }
        }

        UI.Label {
            visible: queue.count === 0
            x: 20
            width: queue.width - 40
            text: qsTr("Nothing is queued. Play an album, a playlist or a song and the rest of it lines up here.")
            size: 13
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
        }
    }
}
