import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// One wallpaper to pick: the thumbnail inset in a hairline frame that
// brightens under the pointer; the current one has a 2px accent edge and a
// tick (Personalisation.dc, Home).
T.AbstractButton {
    id: thumb

    property url source
    property bool selected: false
    property bool showTick: true

    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.RadioButton
    Accessible.checked: selected

    background: Item {
        Image {
            anchors.fill: parent
            anchors.margins: 3
            source: thumb.source
            fillMode: Image.PreserveAspectCrop
            sourceSize.width: 320
            asynchronous: true
        }

        Rectangle {
            anchors.fill: parent
            radius: UI.Style.radiusSm
            color: "transparent"
            border.width: thumb.selected ? 2 : 1
            border.color: thumb.selected ? UI.Style.accent : thumb.hovered ? UI.Style.borderControl : UI.Style.borderGlass
            Behavior on border.color {
                ColorAnimation { duration: UI.Style.hoverDuration }
            }
        }

        Rectangle {
            visible: thumb.selected && thumb.showTick
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            width: 22
            height: 22
            radius: 11
            color: UI.Style.accent

            UI.Icon {
                anchors.centerIn: parent
                name: "check"
                size: 14
                strokeWidth: 2.4
                color: UI.Style.textOnAccent
            }
        }

        UI.FocusRing {
            visible: thumb.visualFocus
            targetRadius: UI.Style.radiusSm
        }
    }
}
