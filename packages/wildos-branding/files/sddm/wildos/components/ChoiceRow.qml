import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// A compact glass row under "Sign-in options" (user, session, keyboard
// layout). With `choices` set it shows the current one and opens a Wild OS
// menu of the others below it, where the arrow keys move and Enter picks,
// as a drop-down does; without, it is a single action such as "Switch user".
T.AbstractButton {
    id: row

    property Item backdrop: null
    property string iconName
    property string label
    property string value
    // [{ text, detail, checked, run }]
    property var choices: []

    readonly property bool hasMenu: choices.length > 0

    implicitWidth: 300
    implicitHeight: 38
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: value.length > 0 ? label + ", " + value : label
    Accessible.role: hasMenu ? Accessible.ComboBox : Accessible.Button

    onClicked: {
        if (!hasMenu) {
            return;
        }
        if (menu.opened) {
            menu.close();
        } else {
            menu.entries = choices.map(choice => ({
                text: choice.text,
                shortcut: choice.detail ?? "",
                checked: choice.checked === true,
                run: choice.run
            }));
            menu.popup(row, 0, row.height + 6);
        }
    }
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    background: Item {
        Glass {
            anchors.fill: parent
            backdrop: row.backdrop
            tint: row.down ? UI.Style.glassOnWallpaperPressed
                 : row.hovered || menu.opened ? UI.Style.glassOnWallpaperHover
                 : UI.Style.glassOnWallpaper
            blurRadius: 24
            Behavior on tint { ColorAnimation { duration: UI.Style.hoverDuration } }
        }

        UI.FocusRing {
            visible: row.visualFocus
            targetRadius: row.height / 2
        }
    }

    contentItem: Item {
        UI.Icon {
            id: glyph
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            name: row.iconName
            size: 16
            color: UI.Style.inkOnWallpaper
        }

        UI.Label {
            id: caption
            anchors.left: glyph.right
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: row.label
            color: row.value.length > 0 ? UI.Style.inkOnWallpaperMuted : UI.Style.inkOnWallpaper
        }

        UI.Label {
            anchors.left: caption.right
            anchors.leftMargin: 10
            anchors.right: chevron.visible ? chevron.left : parent.right
            anchors.rightMargin: chevron.visible ? 8 : 14
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight
            text: row.value
            color: UI.Style.inkOnWallpaper
        }

        UI.Icon {
            id: chevron
            visible: row.hasMenu
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            name: "chevron-down"
            size: 14
            strokeWidth: 2.2
            color: UI.Style.inkOnWallpaper
            rotation: menu.opened ? 180 : 0
            Behavior on rotation { NumberAnimation { duration: UI.Style.hoverDuration } }
        }
    }

    UI.Menu {
        id: menu
        width: row.width
        minimumWidth: row.width
        maximumWidth: row.width
    }
}
