import QtQuick

// A right click on anything that stands for an app opens the app's menu
// (See details, Open, Install or Uninstall, Copy link). The window owns the
// one menu, so every tile and row offers the same choices. Declared inside
// a tile, it handles the tile's own pointer events.
TapHandler {
    id: handler

    property string appKey

    acceptedButtons: Qt.RightButton
    onTapped: point => {
        const item = handler.parent;
        const window = item ? item.Window.window : null;
        if (window && window.showAppMenu) {
            window.showAppMenu(handler.appKey, item, point.position.x, point.position.y);
        }
    }
}
