import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// One step of setup: 36 / 40 padding, a 28px title over a 14px line, the
// step's content 22 apart, and the 76px footer with a 12px note at the left
// and the buttons at the right, the main one last.
Item {
    id: page

    property string title
    property string text
    property string note
    property alias leading: leadingHolder.data
    default property alias content: body.data
    property alias buttons: buttonRow.data
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
                    font.weight: Font.DemiBold
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

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.topMargin: page.spacing
            spacing: page.spacing
        }

        Item {
            Layout.fillHeight: true
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

                UI.Label {
                    Layout.fillWidth: true
                    text: page.note
                    size: 12
                    color: UI.Style.inkMuted
                    wrapMode: Text.Wrap
                    elide: Text.ElideNone
                    lineHeight: 1.45
                }

                RowLayout {
                    id: buttonRow
                    spacing: 10
                }
            }
        }
    }
}
