import QtQuick
import QtQuick.Templates as T
import QtQuick.Window
import WildOS.PDF
import WildOS.UI as UI

// The pages down the left (AppPdf.dc): 170px wide, each page 104px across
// with its number under it, the page in view outlined in the accent.
Rectangle {
    id: strip

    property int current: 0
    readonly property var sizes: PdfDocument.pageSizes

    signal pageChosen(int index)

    color: UI.Style.shade(0.25, 0.05)

    onCurrentChanged: list.positionViewAtIndex(current, ListView.Contain)

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: UI.Style.divider
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.rightMargin: 1
        topMargin: 18
        bottomMargin: 18
        spacing: 16
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: PdfDocument.loaded ? PdfDocument.pageCount : 0
        reuseItems: true

        delegate: Item {
            id: thumb
            required property int index
            readonly property var size: strip.sizes[index]
            readonly property bool isCurrent: index === strip.current
            readonly property real frameHeight: size && size.width > 0 ? 104 * size.height / size.width : 147

            width: list.width
            height: frameHeight + 6 + number.height

            Rectangle {
                id: frame
                anchors.horizontalCenter: parent.horizontalCenter
                width: 104
                height: thumb.frameHeight
                color: "#ffffff"
                radius: 2

                Image {
                    anchors.fill: parent
                    source: "image://page/" + PdfDocument.generation + "/" + thumb.index + "/" + Math.round(104 * Screen.devicePixelRatio) + "/" + (PdfDocument.revisions[thumb.index] ?? 0)
                    asynchronous: true
                    smooth: true
                    retainWhileLoading: true
                }

                // The board's outline, 2px out from the page.
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: thumb.isCurrent ? -4 : -3
                    color: "transparent"
                    radius: 4
                    border.width: thumb.isCurrent ? 2 : 1
                    border.color: thumb.isCurrent ? UI.Style.accent : hover.containsMouse ? UI.Style.borderControl : UI.Style.borderSubtle
                }

                MouseArea {
                    id: hover
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Page %1").arg(thumb.index + 1)
                    onClicked: strip.pageChosen(thumb.index)
                }
            }

            UI.Label {
                id: number
                anchors.top: frame.bottom
                anchors.topMargin: 6
                anchors.horizontalCenter: parent.horizontalCenter
                text: thumb.index + 1
                size: 12
                color: thumb.isCurrent ? UI.Style.ink : UI.Style.inkMuted
            }
        }

        T.ScrollBar.vertical: UI.ScrollBar {}
    }
}
