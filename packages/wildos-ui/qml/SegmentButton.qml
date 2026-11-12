import QtQuick
import QtQuick.Templates as T

// A tab drawn as a small pill, for a choice of views inside a panel rather
// than a window's title bar: the clipboard and emoji panel's Emoji, Symbols
// and Clipboard (ClipboardEmoji.dc). 30px tall, 12px either side, radius 8,
// 13px text; the current one sits on a 0.10 fill in semibold, the others
// are muted until hovered. Put them in a Row with spacing 4 and give them a
// shared ButtonGroup, or bind `checked` yourself.
T.TabButton {
    id: control

    property bool previewHover: false
    readonly property bool lit: (hovered || previewHover) && !checked && enabled

    implicitWidth: label.implicitWidth + 24
    implicitHeight: Math.max(30, label.implicitHeight + 10)
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.PageTab
    Accessible.name: text

    background: Rectangle {
        radius: Style.radiusSm
        color: control.checked ? Style.surfaceSelected : control.down ? Style.surfacePressed : control.lit ? Style.surfaceHover : "transparent"
        Behavior on color {
            ColorAnimation { duration: Style.hoverDuration }
        }

        FocusRing {
            visible: control.visualFocus
            targetRadius: parent.radius
        }
    }

    contentItem: Label {
        id: label
        text: control.text
        size: 13
        horizontalAlignment: Text.AlignHCenter
        font.weight: control.checked ? Style.weightSemiBold : Style.weightRegular
        color: control.checked || control.lit ? Style.ink : Style.inkAlpha(0.75, 0.70)
    }
}
