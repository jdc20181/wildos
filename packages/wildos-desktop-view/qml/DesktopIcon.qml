import QtQuick
import QtQuick.Effects
import WildOS.UI as UI

// One icon as DesktopIcons.dc draws it: a 92px tile (at medium size) with
// 8 4 6 padding, the 48px icon, 6px gap and a 12px white label carrying
// the drawn shadow (0 1px 3px rgba(0,0,0,0.75), 0 0 10px rgba(0,0,0,0.35)).
// Selected, the tile fills with the accent at 0.28 inside a 1px edge of
// the accent text colour at 0.55, radius 10, as the design draws Glacier.
Item {
    id: tile

    required property int index
    required property string name
    required property string fileName
    required property url url
    required property string iconName
    required property string kind
    required property bool isDir
    required property bool selected
    required property bool cut
    required property bool preview
    required property var stamp
    required property real posX
    required property real posY
    required property bool dropTarget

    required property var view

    readonly property bool current: view.model.currentIndex === index
    readonly property bool renaming: view.renameRow === index
    readonly property bool dragHover: drops.containsDrag && drops.accepting
    readonly property color accentText: UI.Style.accentText

    x: posX
    y: posY
    width: view.itemWidth
    height: column.implicitHeight + 16
    z: selected || renaming ? 2 : hover.containsMouse ? 1 : 0

    Accessible.role: Accessible.ListItem
    Accessible.name: name
    Accessible.selected: selected

    Rectangle {
        id: plate
        anchors.fill: parent
        radius: 10
        color: tile.selected || tile.dragHover ? UI.Style.accentAlpha(0.28, 0.28)
            : hover.containsMouse ? UI.Style.accentAlpha(0.14, 0.14)
            : "transparent"
        border.width: 1
        border.color: tile.selected || tile.dragHover ? Qt.rgba(tile.accentText.r, tile.accentText.g, tile.accentText.b, 0.55)
            : hover.containsMouse || (tile.current && tile.view.keyboardUsed) ? Qt.rgba(tile.accentText.r, tile.accentText.g, tile.accentText.b, 0.30)
            : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.quickDuration }
        }
    }

    Column {
        id: column
        y: 9
        width: parent.width
        spacing: 6

        ItemIcon {
            id: art
            anchors.horizontalCenter: parent.horizontalCenter
            size: tile.view.iconSize
            kind: tile.kind
            iconName: tile.iconName
            url: tile.url
            preview: tile.preview
            stamp: tile.stamp
            binFull: tile.view.binFull
            faded: tile.cut
        }

        Item {
            x: 4
            width: parent.width - 8
            height: tile.renaming ? editor.height : label.height

            // The design's two text shadows: a 10px glow at 0.35 and a 3px
            // shadow 1px down at 0.75, both under the white label.
            MultiEffect {
                visible: !tile.renaming
                anchors.fill: label
                source: label
                colorization: 1
                colorizationColor: "black"
                blurEnabled: true
                blur: 1
                blurMax: 10
                autoPaddingEnabled: true
                opacity: 0.35
            }

            MultiEffect {
                visible: !tile.renaming
                x: label.x
                y: label.y + 1
                width: label.width
                height: label.height
                source: label
                colorization: 1
                colorizationColor: "black"
                blurEnabled: true
                blur: 1
                blurMax: 3
                autoPaddingEnabled: true
                opacity: 0.75
            }

            Text {
                id: label
                visible: !tile.renaming
                width: parent.width
                text: tile.name
                color: UI.Style.inkOnWallpaper
                font.family: UI.Style.fontFamily
                font.pixelSize: UI.Style.px(12)
                lineHeightMode: Text.FixedHeight
                lineHeight: UI.Style.px(12) * 1.3
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                // A selected name is shown in full, as on Windows.
                maximumLineCount: tile.selected ? 6 : 2
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }

            RenameField {
                id: editor
                visible: tile.renaming
                width: parent.width + 8
                x: -4
                active: tile.renaming
                original: tile.name
                onCommit: text => tile.view.finishRename(tile.index, text)
                onCancel: tile.view.cancelRename()
            }
        }
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        enabled: !tile.renaming
        cursorShape: Qt.ArrowCursor

        property point pressPoint
        property bool deferSelect: false

        onPressed: mouse => {
            tile.view.takeFocus();
            tile.view.pointerUsed();
            pressPoint = Qt.point(mouse.x, mouse.y);
            deferSelect = false;
            if (mouse.button === Qt.MiddleButton) {
                return;
            }
            if (mouse.button === Qt.RightButton) {
                if (!tile.selected) {
                    tile.view.model.selectOnly(tile.index);
                } else {
                    tile.view.model.currentIndex = tile.index;
                }
                return;
            }
            if (mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) {
                tile.view.model.select(tile.index, mouse.modifiers);
            } else if (tile.selected) {
                // Pressing a selected icon may start dragging the whole
                // selection, so it narrows to this one only on release.
                deferSelect = true;
                tile.view.model.currentIndex = tile.index;
            } else {
                tile.view.model.selectOnly(tile.index);
            }
        }

        onPositionChanged: mouse => {
            if (!(mouse.buttons & Qt.LeftButton) || tile.view.dragging) {
                return;
            }
            if (Math.abs(mouse.x - pressPoint.x) + Math.abs(mouse.y - pressPoint.y) >= Qt.styleHints.startDragDistance) {
                deferSelect = false;
                const p = mapToItem(tile.view, pressPoint.x, pressPoint.y);
                tile.view.startDrag(tile, p);
            }
        }

        onReleased: mouse => {
            if (mouse.button === Qt.LeftButton && deferSelect) {
                tile.view.model.selectOnly(tile.index);
            }
            deferSelect = false;
        }

        onClicked: mouse => {
            if (mouse.button === Qt.RightButton) {
                const p = mapToItem(tile.view, mouse.x, mouse.y);
                tile.view.openItemMenu(p.x, p.y);
            }
        }

        onDoubleClicked: mouse => {
            if (mouse.button === Qt.LeftButton && !(mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))) {
                tile.view.openSelection();
            }
        }
    }

    DropArea {
        id: drops
        anchors.fill: parent
        enabled: tile.dropTarget
        readonly property bool accepting: tile.dropTarget && !(tile.view.dragging && tile.selected)

        onEntered: drag => {
            if (!accepting) {
                drag.accepted = false;
            }
        }
        onDropped: drop => tile.view.dropOnItem(tile.index, drop)
    }
}
