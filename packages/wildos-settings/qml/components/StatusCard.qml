import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// The tinted card at the top of a page that says, in one sentence, whether
// everything is fine. It turns sand when something needs a look.
Rectangle {
    id: card

    property string iconName: "shield-check"
    property string title
    property string description
    property bool warning: false
    property bool spinning: false
    property int iconCircle: 64
    property int iconSize: 32
    property real iconStroke: 2.2
    // The design gives the sentence a 1.5 line height on Privacy and the
    // font's own on Updates; 0 keeps the font's.
    property real descriptionLineHeight: 0
    default property alias actions: actionRow.data

    readonly property color tint: warning ? UI.Style.warning : UI.Style.accent

    Layout.fillWidth: true
    implicitHeight: Math.max(iconCircle + 46, layout.implicitHeight + 46)
    radius: UI.Style.radiusLg
    color: warning ? UI.Style.warningSoft : UI.Style.accentSoft
    border.width: 1
    border.color: warning ? Qt.rgba(tint.r, tint.g, tint.b, 0.30) : UI.Style.accentBorder

    FontMetrics {
        id: fontMetrics
        font.family: UI.Style.fontFamily
        font.pixelSize: UI.Style.fontBodySm
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        spacing: 20

        Rectangle {
            Layout.preferredWidth: card.iconCircle
            Layout.preferredHeight: card.iconCircle
            radius: card.iconCircle / 2
            color: card.tint

            UI.Icon {
                id: statusIcon
                anchors.centerIn: parent
                name: card.iconName
                size: card.iconSize
                strokeWidth: card.iconStroke
                color: UI.Style.textOnAccent

                RotationAnimator {
                    target: statusIcon
                    running: card.spinning
                    from: 0
                    to: 360
                    duration: 1200
                    loops: Animation.Infinite
                    onRunningChanged: if (!running) statusIcon.rotation = 0
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            UI.Label {
                Layout.fillWidth: true
                text: card.title
                size: 18
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                elide: Text.ElideNone
            }

            UI.Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: card.description
                lineHeightMode: Text.FixedHeight
                lineHeight: card.descriptionLineHeight > 0 ? card.descriptionLineHeight * UI.Style.textScale : Math.ceil(fontMetrics.height)
                color: UI.Style.inkStatus
                wrapMode: Text.Wrap
                elide: Text.ElideNone
            }
        }

        Row {
            id: actionRow
            spacing: 8
        }
    }
}
