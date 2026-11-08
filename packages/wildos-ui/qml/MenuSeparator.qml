import QtQuick
import QtQuick.Templates as T

// A hairline between groups of menu rows, 8px in from each side with 4px
// above and below (MenusShell.dc).
T.MenuSeparator {
    implicitWidth: 40
    implicitHeight: 9
    padding: 0

    contentItem: Item {
        Rectangle {
            x: 8
            width: parent.width - 16
            height: 1
            anchors.verticalCenter: parent.verticalCenter
            color: Style.tone(0.10)
        }
    }
}
