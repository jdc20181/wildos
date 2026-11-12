import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import WildOS.UI as UI

// One step of setup, in the setup window and in first run's step 7: 36 / 40
// padding, a 28px title over a 14px line, the step's content 22 apart (16
// on the denser boards, 3.9, 3.10 and 9.4), and the 76px footer with a 12px
// note at the left, or whatever footerLeading holds there instead, and the
// buttons at the right, the main one last.
Item {
    id: page

    property string title
    property string text
    property string note
    property alias leading: leadingHolder.data
    default property alias content: body.data
    property alias buttons: buttonRow.data
    property alias footerLeading: footerLeadingHolder.data
    // 22 as drawn; a step with more rows than the boards closes up a little
    // rather than run into the footer.
    property int spacing: 22

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 40
        anchors.rightMargin: 40
        anchors.topMargin: 36
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            Item {
                id: leadingHolder
                visible: children.length > 0
                implicitWidth: childrenRect.width
                implicitHeight: childrenRect.height
            }

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
                    visible: page.text.length > 0
                    text: page.text
                    size: 14
                    color: UI.Style.inkStatus
                    wrapMode: Text.Wrap
                    elide: Text.ElideNone
                    lineHeightMode: Text.FixedHeight
                    lineHeight: Math.round(UI.Style.px(14) * 1.5)
                }
            }
        }

        // The step's content scrolls when it is taller than the room above
        // the footer, so a longer step never pushes its buttons out of the
        // window.
        Flickable {
            id: scroller
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: page.spacing
            Layout.rightMargin: -14
            contentWidth: width
            contentHeight: body.implicitHeight + 16
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: UI.ScrollBar {}

            ColumnLayout {
                id: body
                width: scroller.width - 14
                spacing: page.spacing
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.leftMargin: -40
            Layout.rightMargin: -40
            implicitHeight: 76

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: UI.Style.tone(0.08)
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 40
                anchors.rightMargin: 40
                spacing: 10

                Item {
                    id: footerLeadingHolder
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // A note (an error, say) takes the place back.
                    visible: children.length > 0 && page.note.length === 0
                }

                UI.Label {
                    Layout.fillWidth: true
                    visible: !footerLeadingHolder.visible
                    text: page.note
                    size: 12
                    color: UI.Style.inkMuted
                    wrapMode: Text.Wrap
                    elide: Text.ElideNone
                    lineHeightMode: Text.FixedHeight
                    lineHeight: Math.round(UI.Style.px(size) * 1.45)
                }

                RowLayout {
                    id: buttonRow
                    spacing: 10
                }
            }
        }
    }
}
