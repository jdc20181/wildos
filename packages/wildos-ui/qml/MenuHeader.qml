import QtQuick
import QtQuick.Templates as T

// The small capitals over a group of menu rows, "Pinned" and "Recent" in a
// taskbar jump list (MenusShell.dc): 28px, 11px 600 text in capitals with
// 0.04em tracking, 10px in. Menus pass over it like a separator, so arrow
// keys never stop on it.
T.MenuSeparator {
    id: control

    property string text

    implicitWidth: 38 + title.implicitWidth + 12
    implicitHeight: Math.max(Style.menuHeaderHeight, title.implicitHeight + 12)
    padding: 0
    Accessible.role: Accessible.StaticText
    Accessible.name: text

    contentItem: Item {
        Label {
            id: title
            x: 10
            width: parent.width - 20
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            size: 11
            font.weight: Style.weightSemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: Style.px(11) * 0.04
            color: Style.inkPlaceholder
        }
    }
}
