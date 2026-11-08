import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// What a page says when its list is empty (EmptyStates.dc): an icon on an
// accent disc, why in one line, what to do in another, and one button.
ColumnLayout {
    id: empty

    property string heading
    property string explanation
    property string iconPath
    property string iconFill
    property string actionText
    property bool busy: false

    signal action()

    spacing: 12

    Rectangle {
        visible: empty.iconPath.length > 0
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 64
        Layout.preferredHeight: 64
        radius: 32
        color: UI.Style.accentSoft

        UI.Glyph {
            anchors.centerIn: parent
            width: 30
            height: 30
            path: empty.iconPath
            fillPath: empty.iconFill
            strokeWidth: 1.9
            color: UI.Style.accentText
        }
    }

    UI.Label {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: empty.heading
        size: 15
        font.weight: Font.DemiBold
    }

    UI.Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: Math.min(280, empty.width)
        horizontalAlignment: Text.AlignHCenter
        text: empty.explanation
        visible: text.length > 0
        color: UI.Style.inkMuted
        lineHeight: 1.5
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
    }

    UI.Button {
        visible: empty.actionText.length > 0
        Layout.alignment: Qt.AlignHCenter
        text: empty.actionText
        busy: empty.busy
        onClicked: empty.action()
    }
}
