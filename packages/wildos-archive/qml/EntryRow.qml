import QtQuick
import WildOS.UI as UI

// A row of the table (AppArchive.dc): 38px, 16px in from the list's edges
// and 12px of padding, radius 6, the check box, an 18px icon or a 22px
// thumbnail at radius 4, the name, then Size and Packed (80, right
// aligned), Modified (140, 24 in) and Type (100), 12px apart.
Item {
    id: row

    required property int index
    required property int entry
    required property string name
    required property bool isDir
    required property string sizeText
    required property string packedText
    required property string modifiedText
    required property string typeText
    required property string kind
    required property bool checked
    required property string thumb

    signal pressed(int modifiers)
    signal opened()
    signal toggled()
    signal menuRequested(real x, real y)

    implicitHeight: 38

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        radius: 6
        color: mouse.pressed ? ArchiveStyle.rowPressed : row.checked ? ArchiveStyle.rowSelected : mouse.containsMouse ? ArchiveStyle.rowHover : "transparent"

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: event => {
                if (event.button === Qt.RightButton && !row.checked) {
                    row.pressed(0);
                }
            }
            onClicked: event => {
                if (event.button === Qt.RightButton) {
                    row.menuRequested(event.x + 16, event.y);
                } else {
                    row.pressed(event.modifiers);
                }
            }
            onDoubleClicked: event => {
                if (event.button === Qt.LeftButton) {
                    row.opened();
                }
            }
        }

        Row {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            CheckMark {
                anchors.verticalCenter: parent.verticalCenter
                checked: row.checked
                onClicked: row.toggled()
                Accessible.name: qsTr("Select %1").arg(row.name)
            }

            Item {
                anchors.verticalCenter: parent.verticalCenter
                width: row.kind === "image" ? 22 : 18
                height: width

                Rectangle {
                    anchors.fill: parent
                    visible: row.kind === "image"
                    radius: 4
                    color: ArchiveStyle.thumbBlank
                    clip: true

                    Image {
                        anchors.fill: parent
                        visible: row.thumb.length > 0
                        source: row.thumb
                        sourceSize.width: 44
                        sourceSize.height: 44
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        smooth: true
                    }
                    ArchiveIcon {
                        anchors.centerIn: parent
                        visible: row.thumb.length === 0
                        name: "image"
                        size: 14
                        color: ArchiveStyle.fileInk
                    }
                }
                ArchiveIcon {
                    anchors.centerIn: parent
                    visible: row.kind !== "image"
                    name: row.kind === "folder" ? "folder" : row.kind === "link" ? "link" : row.kind === "unsafe" ? "warning" : "file-text"
                    size: 18
                    strokeWidth: 1.8
                    color: row.kind === "folder" ? ArchiveStyle.folderInk : row.kind === "unsafe" ? UI.Style.danger : ArchiveStyle.fileInk
                }
            }

            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - x - 80 - 80 - 140 - 100 - 4 * 12
                text: row.name
                size: 13
                elide: Text.ElideMiddle
            }
            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                width: 80
                horizontalAlignment: Text.AlignRight
                text: row.sizeText
                size: 13
                color: UI.Style.inkMuted
            }
            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                width: 80
                horizontalAlignment: Text.AlignRight
                text: row.packedText
                size: 13
                color: UI.Style.inkMuted
            }
            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                width: 140
                leftPadding: 24
                text: row.modifiedText
                size: 13
                color: UI.Style.inkMuted
                elide: Text.ElideRight
            }
            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                width: 100
                text: row.typeText
                size: 13
                color: UI.Style.inkMuted
                elide: Text.ElideRight
            }
        }
    }
}
