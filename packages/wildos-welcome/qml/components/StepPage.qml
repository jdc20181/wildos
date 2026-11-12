import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import WildOS.UI as UI

// One step of the flow: the heading and its line of explanation, the
// step's own content below with the design's 22px rhythm, and the footer
// bar across the bottom.
Item {
    id: page

    property string title
    property string subtitle
    property int gap: 22
    property alias note: noteText.text
    property color noteColor: UI.Style.inkMuted
    default property alias content: body.data
    property alias buttons: buttonRow.data

    // Gives a button the keyboard without drawing its focus ring; the ring
    // appears once Tab is used, as on Windows.
    function focusQuietly(item) {
        item.forceActiveFocus(Qt.OtherFocusReason);
    }

    // What the sheet needs to show the whole step without scrolling.
    implicitHeight: 36 + body.implicitHeight + 24 + footer.height

    Flickable {
        id: scroller
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footer.top
        contentHeight: 36 + body.implicitHeight + 24
        boundsBehavior: Flickable.StopAtBounds
        clip: contentHeight > height
        T.ScrollBar.vertical: UI.ScrollBar {}

        ColumnLayout {
            id: body
            x: 40
            y: 36
            width: scroller.width - 80
            spacing: page.gap

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                UI.Label {
                    Layout.fillWidth: true
                    text: page.title
                    size: 28
                    font.weight: UI.Style.weightSemiBold
                    font.letterSpacing: -0.28
                    Accessible.role: Accessible.Heading
                }

                UI.Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: page.subtitle
                    size: 14
                    lineHeight: 21 * UI.Style.textScale
                    lineHeightMode: Text.FixedHeight
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    color: UI.Style.inkStatus
                }
            }
        }
    }

    Item {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Math.max(76, Math.ceil(footerRow.implicitHeight) + 36)

        Rectangle {
            width: parent.width
            height: 1
            color: UI.Style.tone(0.08)
        }

        // Windows puts the buttons bottom right with the main action last
        // in reading order, nearest the corner.
        RowLayout {
            id: footerRow
            anchors.fill: parent
            anchors.leftMargin: 40
            anchors.rightMargin: 40
            spacing: 10

            UI.Label {
                id: noteText
                Layout.fillWidth: true
                size: 12
                color: page.noteColor
                wrapMode: Text.WordWrap
                maximumLineCount: 2
            }

            Row {
                id: buttonRow
                spacing: 10
            }
        }
    }
}
