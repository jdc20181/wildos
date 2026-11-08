import QtQuick
import QtQuick.Window
import WildOS.UI as UI

// Where a dragged window will land when dropped on a zone (Snap.dc): an
// accent-tinted rounded area with a 2px accent edge. Drawn by the effect
// itself rather than through KWin's shared outline, which is the stock
// Plasma frame and which KWin's own edge tiling hides and shows at will.
Window {
    id: preview

    readonly property bool _q_showWithoutActivating: true

    function showAt(r: rect): void {
        preview.x = r.x;
        preview.y = r.y;
        preview.width = Math.max(1, r.width);
        preview.height = Math.max(1, r.height);
        preview.visible = true;
    }

    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.BypassWindowManagerHint | Qt.WindowDoesNotAcceptFocus
           | Qt.WindowTransparentForInput

    Rectangle {
        anchors.fill: parent
        radius: UI.Style.windowRadius
        color: UI.Style.accentAlpha(0.16, 0.12)
        border.width: 2
        border.color: UI.Style.accentAlpha(0.70, 0.60)
    }
}
