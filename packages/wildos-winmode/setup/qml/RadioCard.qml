import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import WildOS.UI as UI

// One choice of 9.2, 9.3 and 9.10: 14 / 18 padding, radius 12, a 20px
// radio at the top left, a 14px title with an optional status chip, a 13px
// description and anything else under it (a link, the key field). Chosen:
// a 2px accent edge on accent at 0.08. A choice that cannot be made says
// why in `reason` and is shown disabled, never hidden.
T.AbstractButton {
    id: card

    property string title
    property string description
    property string chip
    property string chipTone: "success"
    property string reason
    default property alias extra: extraColumn.data

    readonly property bool lit: hovered && enabled

    Layout.fillWidth: true
    checkable: true
    autoExclusive: true
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    implicitHeight: row.implicitHeight + 28
    Accessible.role: Accessible.RadioButton
    Accessible.name: title
    Accessible.description: reason.length > 0 ? reason : description

    background: Rectangle {
        radius: UI.Style.radiusMd
        color: card.checked ? UI.Style.accentAlpha(0.08, 0.07) : card.lit ? UI.Style.surfaceHover : UI.Style.surfaceRest
        border.width: card.checked ? 2 : 1
        border.color: card.checked ? UI.Style.accent : UI.Style.tone(0.10)

        UI.FocusRing {
            visible: card.visualFocus
            targetRadius: UI.Style.radiusMd
        }
    }

    contentItem: Item {
        RowLayout {
            id: row
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 18 - card.leftPadding
            anchors.rightMargin: 18 - card.rightPadding
            anchors.verticalCenter: parent.verticalCenter
            spacing: 14

            Rectangle {
                Layout.alignment: Qt.AlignTop
                implicitWidth: 20
                implicitHeight: 20
                radius: 10
                color: "transparent"
                border.width: 2
                border.color: card.checked ? UI.Style.accent : UI.Style.radioRing
                opacity: card.enabled ? 1 : 0.5

                Rectangle {
                    anchors.centerIn: parent
                    visible: card.checked
                    width: 10
                    height: 10
                    radius: 5
                    color: UI.Style.accent
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    spacing: 8

                    UI.Label {
                        text: card.title
                        size: 14
                        font.weight: Font.DemiBold
                        color: card.enabled ? UI.Style.ink : UI.Style.inkDisabled
                    }

                    UI.Chip {
                        visible: card.chip.length > 0
                        text: card.chip
                        tone: card.chipTone
                    }
                }

                UI.Label {
                    Layout.fillWidth: true
                    visible: card.description.length > 0
                    text: card.description
                    size: 13
                    color: card.enabled ? UI.Style.inkAlpha(0.78, 0.72) : UI.Style.inkDisabled
                    wrapMode: Text.Wrap
                    elide: Text.ElideNone
                    lineHeight: 1.45
                }

                UI.Label {
                    Layout.fillWidth: true
                    visible: card.reason.length > 0
                    text: card.reason
                    size: 12
                    color: UI.Style.warning
                    wrapMode: Text.Wrap
                    elide: Text.ElideNone
                }

                ColumnLayout {
                    id: extraColumn
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    visible: children.length > 0
                    spacing: 6
                }
            }
        }
    }

    padding: 0
    implicitWidth: 400
}
