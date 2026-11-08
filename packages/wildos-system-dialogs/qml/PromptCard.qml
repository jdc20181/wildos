import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import WildOS.UI as UI

// One system prompt as SystemPrompts.dc draws them: dialog glass at radius
// 14 with a 0.12 edge and the 0 30px 80px shadow; a 40px picture beside a
// 16px title and a 13px line or two (20 / 22 padding, 16 apart); a footer
// band (rgba(6,8,20,0.18), hairline above, 12 / 22 padding) with the
// buttons at the right, the safe or main choice last.
Item {
    id: card

    property string title
    property string text
    property alias picture: pictureHolder.data
    property alias extra: extraColumn.data
    property alias buttons: buttonRow.data

    implicitWidth: 440
    implicitHeight: column.implicitHeight

    Rectangle {
        id: glass
        anchors.fill: parent
        radius: UI.Style.radiusLg
        color: UI.Style.glassDialog
        border.width: 1
        border.color: UI.Style.borderGlass
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: UI.Style.shadowFlyout
            shadowVerticalOffset: 30
            shadowBlur: 1.0
            blurMax: 64
        }
    }

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            Layout.topMargin: 20
            Layout.bottomMargin: 20
            spacing: 16

            Item {
                id: pictureHolder
                Layout.alignment: Qt.AlignTop
                implicitWidth: 40
                implicitHeight: 40
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                UI.Label {
                    Layout.fillWidth: true
                    text: card.title
                    size: 16
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    elide: Text.ElideNone
                    Accessible.role: Accessible.Heading
                }

                UI.Label {
                    Layout.fillWidth: true
                    visible: card.text.length > 0
                    text: card.text
                    size: 13
                    color: UI.Style.inkStatus
                    lineHeightMode: Text.FixedHeight
                    lineHeight: Math.round(UI.Style.px(13) * 1.5)
                    wrapMode: Text.Wrap
                    elide: Text.ElideNone
                }

                ColumnLayout {
                    id: extraColumn
                    Layout.fillWidth: true
                    visible: children.length > 0
                    spacing: 6
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: buttonRow.implicitHeight + 24
            bottomLeftRadius: UI.Style.radiusLg
            bottomRightRadius: UI.Style.radiusLg
            color: UI.Style.shade(0.18, 0.04)

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: UI.Style.divider
            }

            RowLayout {
                id: buttonRow
                anchors.right: parent.right
                anchors.rightMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10
            }
        }
    }
}
