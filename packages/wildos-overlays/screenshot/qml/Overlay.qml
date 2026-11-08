pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import QtQuick.Window
import WildOS.UI as UI
import org.wildos.overlays
import WildOS.Screenshot

// One screen frozen under a dimmed layer (Screenshot.dc). Rectangle: drag
// out an area; window: the window under the pointer lights up, click it;
// full screen: click anywhere; freehand: draw round what you want. What is
// being picked shows at full brightness with a 2px accent edge and its
// size in pixels below it. Escape or the cross leaves without a capture.
Window {
    id: overlay

    required property var screenInfo
    readonly property string screenName: screenInfo ? screenInfo.name : ""
    readonly property bool primary: screenName === ScreenshotApp.primaryScreen
    readonly property real scale: screenInfo ? screenInfo.devicePixelRatio : 1
    readonly property string mode: ScreenshotApp.mode

    // The window under the pointer, in this screen's coordinates.
    property var hoveredWindow: null
    property rect selection: Qt.rect(0, 0, 0, 0)
    property bool dragging: false
    property var freehandPoints: []
    property point pointer: Qt.point(-1, -1)

    screen: screenInfo
    width: screenInfo ? screenInfo.width : 0
    height: screenInfo ? screenInfo.height : 0
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: UI.Style.ground
    visible: false
    title: "Screenshot"

    OverlayPlacement {
        id: placement
        window: overlay
        edges: OverlayPlacement.Top | OverlayPlacement.Bottom | OverlayPlacement.Left | OverlayPlacement.Right
        layer: OverlayPlacement.Overlay
        keyboard: overlay.primary ? OverlayPlacement.Exclusive : OverlayPlacement.OnDemand
        scope: "wildos-screenshot"
    }

    Connections {
        target: ScreenshotApp
        function onSelectingChanged() {
            if (ScreenshotApp.selecting) {
                overlay.reset();
                // Under X11 only full screen gets past the taskbar's reserved
                // strip; on Wayland the layer surface covers the output.
                if (placement.wayland) {
                    overlay.visible = true;
                } else {
                    overlay.showFullScreen();
                }
                if (overlay.primary) {
                    overlay.requestActivate();
                    surface.forceActiveFocus();
                }
            } else {
                overlay.visible = false;
            }
        }
    }

    function reset(): void {
        selection = Qt.rect(0, 0, 0, 0);
        dragging = false;
        freehandPoints = [];
        hoveredWindow = null;
        pointer = Qt.point(-1, -1);
    }

    function windowAt(x: real, y: real): var {
        const list = ScreenshotApp.windows;
        const gx = x + screenInfo.virtualX;
        const gy = y + screenInfo.virtualY;
        for (let i = list.length - 1; i >= 0; i--) {
            const w = list[i];
            if (gx >= w.x && gx < w.x + w.width && gy >= w.y && gy < w.y + w.height) {
                return w;
            }
        }
        return null;
    }

    // What is lit: the dragged area, the hovered window or the whole screen.
    readonly property rect lit: {
        if (mode === "screen") {
            return pointer.x >= 0 ? Qt.rect(0, 0, width, height) : Qt.rect(0, 0, 0, 0);
        }
        if (mode === "window" && hoveredWindow) {
            const w = hoveredWindow;
            const x = Math.max(0, w.x - screenInfo.virtualX);
            const y = Math.max(0, w.y - screenInfo.virtualY);
            const right = Math.min(width, w.x - screenInfo.virtualX + w.width);
            const bottom = Math.min(height, w.y - screenInfo.virtualY + w.height);
            return Qt.rect(x, y, Math.max(0, right - x), Math.max(0, bottom - y));
        }
        if (mode === "rectangle" || (mode === "window" && dragging)) {
            return selection;
        }
        return Qt.rect(0, 0, 0, 0);
    }

    Item {
        id: surface
        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: ScreenshotApp.cancel()
        Keys.onReturnPressed: {
            if (overlay.mode === "screen") {
                ScreenshotApp.takeScreen(overlay.screenName);
            }
        }

        Image {
            id: frozen
            anchors.fill: parent
            source: overlay.visible ? "image://frozen/" + overlay.screenName + "/" + ScreenshotApp.serial : ""
            cache: false
            smooth: true
            fillMode: Image.Stretch
        }

        // The screen stays readable but clearly paused (rgba(4,8,20,0.55)).
        Rectangle {
            anchors.fill: parent
            color: UI.Style.shade(0.55, 0.40)
        }

        // The part being picked, at full brightness.
        Item {
            x: overlay.lit.x
            y: overlay.lit.y
            width: overlay.lit.width
            height: overlay.lit.height
            visible: width > 0 && height > 0
            clip: true

            Image {
                x: -parent.x
                y: -parent.y
                width: overlay.width
                height: overlay.height
                source: frozen.source
                cache: false
                smooth: true
            }
        }

        Rectangle {
            x: overlay.lit.x
            y: overlay.lit.y
            width: overlay.lit.width
            height: overlay.lit.height
            visible: width > 0 && height > 0
            color: "transparent"
            border.width: 2
            border.color: UI.Style.accent
        }

        // Freehand: the drawn shape lit through a mask, with an accent line.
        Item {
            id: freehandLit
            anchors.fill: parent
            visible: overlay.mode === "freehand" && overlay.freehandPoints.length > 2

            Image {
                id: litCopy
                anchors.fill: parent
                source: frozen.source
                cache: false
                visible: false
            }

            Shape {
                id: freehandMask
                anchors.fill: parent
                visible: false
                layer.enabled: true
                ShapePath {
                    strokeColor: "transparent"
                    fillColor: UI.Style.ink
                    PathPolyline { path: overlay.freehandPoints }
                }
            }

            MultiEffect {
                anchors.fill: parent
                source: litCopy
                maskEnabled: true
                maskSource: freehandMask
            }

            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: UI.Style.accent
                    strokeWidth: 2
                    fillColor: "transparent"
                    joinStyle: ShapePath.RoundJoin
                    capStyle: ShapePath.RoundCap
                    PathPolyline { path: overlay.freehandPoints }
                }
            }
        }

        // The size of what you get, in pixels (520 × 300).
        Rectangle {
            readonly property bool show: overlay.lit.width > 0 && overlay.mode !== "screen"
            visible: show
            x: Math.min(overlay.lit.x, overlay.width - width - 8)
            y: overlay.lit.y + overlay.lit.height + 8 + height < overlay.height
                ? overlay.lit.y + overlay.lit.height + 8 : overlay.lit.y + overlay.lit.height - height - 8
            width: sizeLabel.implicitWidth + 16
            height: sizeLabel.implicitHeight + 6
            radius: UI.Style.radiusXs
            color: UI.Style.glass(16, 20, 38, 0.85, 0.92)

            UI.Label {
                id: sizeLabel
                anchors.centerIn: parent
                size: 12
                font.features: { "tnum": 1 }
                text: Math.round(overlay.lit.width * overlay.scale) + " × " + Math.round(overlay.lit.height * overlay.scale)
            }
        }

        MouseArea {
            id: picker
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: overlay.mode === "window" || overlay.mode === "screen" ? Qt.PointingHandCursor : Qt.CrossCursor
            property point origin

            onPositionChanged: mouse => {
                overlay.pointer = Qt.point(mouse.x, mouse.y);
                if (overlay.mode === "window" && !overlay.dragging) {
                    overlay.hoveredWindow = overlay.windowAt(mouse.x, mouse.y);
                }
                if (!pressed) {
                    return;
                }
                if (overlay.mode === "freehand") {
                    const points = overlay.freehandPoints;
                    const last = points[points.length - 1];
                    if (!last || Math.abs(last.x - mouse.x) + Math.abs(last.y - mouse.y) >= 2) {
                        overlay.freehandPoints = points.concat([Qt.point(mouse.x, mouse.y)]);
                    }
                    return;
                }
                if (overlay.mode === "rectangle" || overlay.mode === "window") {
                    const dx = mouse.x - origin.x;
                    const dy = mouse.y - origin.y;
                    // A drag in window mode, as in Snipping Tool, falls back
                    // to an area.
                    if (overlay.mode === "window" && !overlay.dragging && Math.abs(dx) + Math.abs(dy) < 6) {
                        return;
                    }
                    overlay.dragging = true;
                    overlay.hoveredWindow = null;
                    overlay.selection = Qt.rect(Math.min(origin.x, mouse.x), Math.min(origin.y, mouse.y), Math.abs(dx), Math.abs(dy));
                }
            }
            onExited: {
                overlay.pointer = Qt.point(-1, -1);
                overlay.hoveredWindow = null;
            }
            onPressed: mouse => {
                origin = Qt.point(mouse.x, mouse.y);
                overlay.dragging = false;
                if (overlay.mode === "freehand") {
                    overlay.freehandPoints = [Qt.point(mouse.x, mouse.y)];
                } else if (overlay.mode === "rectangle") {
                    overlay.selection = Qt.rect(mouse.x, mouse.y, 0, 0);
                }
            }
            onReleased: mouse => {
                if (overlay.mode === "freehand") {
                    const points = overlay.freehandPoints;
                    if (points.length > 2) {
                        ScreenshotApp.takeFreehand(overlay.screenName, points);
                    }
                    return;
                }
                if (overlay.mode === "screen") {
                    ScreenshotApp.takeScreen(overlay.screenName);
                    return;
                }
                if (overlay.dragging || overlay.mode === "rectangle") {
                    const r = overlay.selection;
                    if (r.width >= 4 && r.height >= 4) {
                        ScreenshotApp.takeArea(overlay.screenName, r);
                    } else {
                        overlay.selection = Qt.rect(0, 0, 0, 0);
                    }
                    overlay.dragging = false;
                    return;
                }
                const w = overlay.windowAt(mouse.x, mouse.y);
                if (w) {
                    ScreenshotApp.takeWindow(w.id, Qt.rect(w.x, w.y, w.width, w.height));
                }
            }
        }

        // Window mode has nothing to light when KWin could not list the
        // windows; say so rather than leave clicks doing nothing.
        Rectangle {
            visible: overlay.mode === "window" && overlay.primary && ScreenshotApp.windows.length === 0 && !overlay.dragging
            anchors.horizontalCenter: parent.horizontalCenter
            y: toolbar.y + toolbar.height + 12
            width: hint.implicitWidth + 28
            height: 36
            radius: 18
            color: UI.Style.glass(16, 20, 38, 0.85, 0.92)

            UI.Label {
                id: hint
                anchors.centerIn: parent
                size: 12
                text: "No windows to pick here. Drag over an area instead."
            }
        }

        Toolbar {
            id: toolbar
            visible: overlay.primary
            anchors.horizontalCenter: parent.horizontalCenter
            y: 24
        }
    }
}
