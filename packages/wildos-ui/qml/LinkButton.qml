import QtQuick
import QtQuick.Templates as T

// Accent-coloured words that act, for actions inside a sentence or a card
// ("What's new", "Learn more"). wildos-settings LinkButton, without the grey
// block that flashed behind the words when pressed.
T.AbstractButton {
    id: control

    property int size: 13
    property bool previewHover: false
    property bool previewFocus: false

    implicitWidth: label.implicitWidth + 8
    implicitHeight: label.implicitHeight + 6
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.name: text
    Accessible.role: Accessible.Link

    background: Item {
        FocusRing {
            visible: control.visualFocus || control.previewFocus
            targetRadius: Style.radiusXs
        }
    }

    contentItem: Label {
        id: label
        horizontalAlignment: Text.AlignHCenter
        text: control.text
        size: control.size
        color: Style.accentText
        opacity: control.down ? 0.75 : 1
        font.underline: control.hovered || control.previewHover
    }
}
