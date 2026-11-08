import QtQuick
import WildOS.Files

// What a file or folder does under the pointer in either view: Windows
// selection rules, open on double click, the context menu, middle click
// for a new tab, dragging out and dropping into folders.
Item {
    id: item

    required property var view
    required property int index
    required property string name
    required property string fileName
    required property string iconName
    required property url url
    required property bool isDir
    required property bool isDrive
    required property string col2
    required property string col3
    required property string sizeText
    required property bool selected
    required property bool preview
    required property var modified
    required property bool faded
    required property string subtitle
    required property real usage
    property bool renaming: false
    readonly property bool hovered: mouse.containsMouse
    readonly property bool highlighted: hovered || drop.containsDrag
    readonly property bool isCurrentFocus: view.keyboardUsed && view.folder && view.folder.currentIndex === index && !selected

    Accessible.role: Accessible.ListItem
    Accessible.name: name
    Accessible.selected: selected

    // The thing actually dragged; Qt moves it, the drag image is the icon.
    Item {
        id: proxy
        width: 1
        height: 1
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.CopyAction | Qt.MoveAction | Qt.LinkAction
        Drag.proposedAction: Qt.MoveAction
        Drag.imageSource: "image://icon/" + encodeURIComponent(item.iconName)
        Drag.imageSourceSize: Qt.size(48, 48)
        Drag.hotSpot.x: 24
        Drag.hotSpot.y: 24
        Drag.active: mouse.drag.active
        Drag.onDragFinished: {
            proxy.x = 0;
            proxy.y = 0;
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        enabled: !item.renaming
        drag.target: item.isDrive ? null : proxy
        drag.threshold: 8
        property bool pendingSingle: false

        onPressed: m => {
            item.view.keyboardUsed = false;
            item.view.focusContent();
            if (m.button === Qt.MiddleButton) {
                return;
            }
            if (m.button === Qt.RightButton) {
                if (!item.selected) {
                    item.view.folder.select(item.index, 0);
                }
                return;
            }
            pendingSingle = item.selected && !(m.modifiers & (Qt.ControlModifier | Qt.ShiftModifier));
            if (!pendingSingle) {
                item.view.folder.select(item.index, m.modifiers);
            }
            proxy.Drag.mimeData = {
                "text/uri-list": item.view.folder.selectedUrls().map(u => u.toString()).join("\r\n")
            };
        }
        onReleased: m => {
            if (pendingSingle && !drag.active && m.button === Qt.LeftButton) {
                item.view.folder.select(item.index, 0);
            }
            pendingSingle = false;
        }
        onClicked: m => {
            if (m.button === Qt.MiddleButton) {
                if (item.isDir && !item.isDrive) {
                    item.view.newTab(item.url, false);
                }
            } else if (m.button === Qt.RightButton) {
                item.view.openItemMenu(item, m.x, m.y);
            }
        }
        onDoubleClicked: m => {
            if (m.button === Qt.LeftButton && !(m.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))) {
                item.view.folder.activate(item.index);
            }
        }
    }

    DropArea {
        id: drop
        anchors.fill: parent
        enabled: item.isDir && !item.selected
        onEntered: dragEvent => dragEvent.accept(dragEvent.proposedAction)
        onDropped: dropEvent => item.view.dropUrls(dropEvent, item.url)
    }
}
