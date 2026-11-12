import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// Accent words that act ("Exclude a folder", "See them all"): the board
// draws them in the accent itself on dark, where the system link is paler.
T.AbstractButton {
    id: link

    property real size: 13

    implicitWidth: label.implicitWidth
    implicitHeight: label.implicitHeight
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Link
    Accessible.name: text

    contentItem: UI.Label {
        id: label
        text: link.text
        size: link.size
        color: BackupStyle.link
        font.underline: link.hovered
    }

    UI.FocusRing {
        visible: link.visualFocus
        targetRadius: 4
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
