import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// The tinted card under a step's title: a 44px tile with a 22px line icon,
// a 15px title and a 13px sentence, 16 / 18 padding, radius 12. "success"
// is the green of "Windows licence found", "accent" the blue of "Windows 11
// Home found", "warning" the amber of "No Windows licence".
Rectangle {
    id: banner

    property string tone: "accent"
    property string iconName
    // The key the boards draw is not one of the shared icons.
    readonly property string keyPath: "M4 15a4 4 0 1 0 8 0a4 4 0 1 0 -8 0M11 12l8-8M16 7l2 2M14 9l2 2"
    property string title
    property string text
    default property alias extra: extraColumn.data

    readonly property color ink: tone === "success" ? UI.Style.success : tone === "warning" ? UI.Style.warning : UI.Style.accentText

    Layout.fillWidth: true
    implicitHeight: row.implicitHeight + 32
    radius: UI.Style.radiusMd
    color: Qt.rgba(ink.r, ink.g, ink.b, 0.10)
    border.width: 1
    border.color: Qt.rgba(ink.r, ink.g, ink.b, tone === "accent" ? 0.26 : 0.28)
    Accessible.role: Accessible.StaticText
    Accessible.name: title + ". " + text

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 16

        Rectangle {
            Layout.alignment: Qt.AlignTop
            implicitWidth: 44
            implicitHeight: 44
            radius: UI.Style.radiusMd
            color: UI.Style.surfaceControl

            UI.Icon {
                anchors.centerIn: parent
                visible: banner.iconName.length > 0
                name: banner.iconName
                size: 22
                strokeWidth: 1.9
                color: banner.ink
            }

            UI.Glyph {
                anchors.centerIn: parent
                visible: banner.iconName.length === 0
                width: 22
                height: 22
                path: banner.keyPath
                strokeWidth: 1.9
                color: banner.ink
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            UI.Label {
                Layout.fillWidth: true
                text: banner.title
                size: 15
                font.weight: Font.DemiBold
            }

            UI.Label {
                Layout.fillWidth: true
                visible: banner.text.length > 0
                text: banner.text
                size: 13
                color: UI.Style.inkStatus
                wrapMode: Text.Wrap
                elide: Text.ElideNone
                lineHeight: 1.45
            }

            RowLayout {
                id: extraColumn
                Layout.topMargin: 6
                visible: children.length > 0
                spacing: 10
            }
        }
    }
}
