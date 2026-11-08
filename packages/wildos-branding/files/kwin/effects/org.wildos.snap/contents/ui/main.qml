import QtQuick
import org.kde.kwin as KWin

import "layouts.js" as Layouts

// Wild OS snap layouts for KWin 6.3, which has quick tiling and a tile editor
// but no layout picker. Three ways in, one picker:
//
//  - Hover the maximise button for a moment: the picker drops down under it.
//  - Drag a window to the top edge of the screen: the picker appears at the
//    top centre; drop the window on a zone. Dropping on the edge itself still
//    maximises, as before.
//  - Meta+Z: the picker opens for the active window with keyboard control.
//
// This is a scene effect rather than a KWin script because only effects can
// take the keyboard, which Meta+Z needs. The hover and drag pickers are
// ordinary flyout windows owned by the same effect.
KWin.SceneEffect {
    id: effect

    // The Workspace.clientArea option for the area a window maximises into,
    // which leaves out the taskbar.
    readonly property int maximizeArea: 2

    property var target: null

    // KWin creates one per screen, outside this file's context, so the
    // scene finds this effect through SceneView rather than by id.
    delegate: PickerScene {}

    function eligible(window: var): bool {
        return window !== null && window !== undefined
            && window.normalWindow && !window.specialWindow
            && window.moveable && window.resizeable
            && !window.fullScreen && !window.minimized;
    }

    function areaFor(output: var): rect {
        if (!output) {
            return Qt.rect(0, 0, 0, 0);
        }
        return KWin.Workspace.clientArea(maximizeArea, output, KWin.Workspace.currentDesktop);
    }

    // Wild OS apps that draw their own title bar, so tabs (Files) or search
    // (Store) can sit in it. Each has 46 px window buttons with close at the
    // right edge and maximise beside it; the value is the title bar height.
    readonly property var wildosTitleBars: ({
        "org.wildos.files": 42,
        "org.wildos.settings": 42,
        "org.wildos.store": 52,
        "org.wildos.terminal": 42,
        "org.wildos.taskmanager": 46
    })

    // The maximise button of a window with a server-drawn title bar or a
    // known Wild OS one, or an empty rect for other windows that draw their
    // own (their buttons are unknown).
    function anchorRectFor(window: var): rect {
        if (!eligible(window)) {
            return Qt.rect(0, 0, 0, 0);
        }
        // KWin reports these by their app id, and does not count a title bar
        // the app draws itself as a border, so they are matched by name.
        const ownTitle = wildosTitleBars[window.resourceClass];
        if (ownTitle !== undefined) {
            const f = window.frameGeometry;
            return Qt.rect(f.x + f.width - 92, f.y, 46, ownTitle);
        }
        if (window.noBorder) {
            return Qt.rect(0, 0, 0, 0);
        }
        const frame = window.frameGeometry;
        const titleHeight = window.clientGeometry.y - frame.y;
        if (titleHeight < 16) {
            return Qt.rect(0, 0, 0, 0);
        }
        return Layouts.maximiseButtonRect(frame, titleHeight, window.maximizeMode, effect.configuration);
    }

    function snap(window: var, layoutIndex: int, zoneIndex: int, output: var): void {
        if (!eligible(window) || layoutIndex < 0 || zoneIndex < 0) {
            return;
        }
        const zone = Layouts.layouts[layoutIndex].zones[zoneIndex];
        const screen = output ?? window.output;
        const rect = Layouts.zoneRect(zone, areaFor(screen));
        if (Layouts.sameRect(window.frameGeometry, rect)) {
            return;
        }
        const slot = Layouts.quickTileSlot(zone);
        // Quick tile shortcuts act on the active window on its own screen,
        // and pressing one for the tile a window already has would undo it.
        if (slot.length > 0 && window.output === screen) {
            KWin.Workspace.activeWindow = window;
            KWin.Workspace[slot]();
            return;
        }
        if (window.maximizeMode !== 0) {
            window.setMaximize(false, false);
        }
        if (window.tile) {
            window.tile = null;
        }
        window.frameGeometry = rect;
        KWin.Workspace.activeWindow = window;
    }

    // Meta+Z

    function open(): void {
        const window = KWin.Workspace.activeWindow;
        if (!eligible(window)) {
            return;
        }
        hideFlyout();
        target = window;
        visible = true;
    }

    function close(): void {
        visible = false;
        target = null;
    }

    function choose(layoutIndex: int, zoneIndex: int): void {
        const window = target;
        close();
        snap(window, layoutIndex, zoneIndex, window ? window.output : null);
    }

    KWin.ShortcutHandler {
        name: "Wild OS snap layouts"
        text: "Show snap layouts for the active window"
        sequence: "Meta+Z"
        onActivated: effect.visible ? effect.close() : effect.open()
    }

    // Hover and drag

    property var dragWindow: null
    property var dragHit: null
    property var hoverWindow: null
    property rect hoverButton: Qt.rect(0, 0, 0, 0)

    function inside(p: point, r: rect, margin: real): bool {
        return r.width > 0 && p.x >= r.x - margin && p.x < r.x + r.width + margin
            && p.y >= r.y - margin && p.y < r.y + r.height + margin;
    }

    function hideFlyout(): void {
        dwellTimer.stop();
        leaveTimer.stop();
        zonePreview.visible = false;
        flyout.visible = false;
        flyout.grid.clear();
        flyout.target = null;
        dragHit = null;
    }

    function showHoverFlyout(): void {
        const window = hoverWindow;
        const button = hoverButton;
        const area = KWin.Workspace.clientArea(maximizeArea, window);
        flyout.mode = "hover";
        flyout.target = window;
        flyout.output = window.output;
        flyout.grid.clear();
        flyout.place(Math.max(area.x + 8, Math.min(button.x + button.width / 2 - flyout.panelWidth / 2, area.x + area.width - flyout.panelWidth - 8)),
                     Math.max(area.y + 8, Math.min(button.y + button.height + 6, area.y + area.height - flyout.panelHeight - 8)));
        flyout.visible = true;
    }

    function showDragFlyout(output: var): void {
        const area = areaFor(output);
        flyout.mode = "drag";
        flyout.target = dragWindow;
        flyout.output = output;
        flyout.grid.clear();
        flyout.place(area.x + (area.width - flyout.panelWidth) / 2, area.y + 12);
        flyout.visible = true;
    }

    function trackDrag(p: point): void {
        const output = KWin.Workspace.screenAt(p);
        if (!output) {
            return;
        }
        const area = areaFor(output);
        // KWin maximises a window released within 5px of the top; the
        // picker opens on the same gesture and sits just below that band.
        const atTop = p.y <= area.y + 5;
        const showing = flyout.visible && flyout.mode === "drag" && flyout.output === output;
        if (!showing) {
            if (!atTop) {
                return;
            }
            showDragFlyout(output);
        } else if (!atTop && !inside(p, flyout.globalRect(), 24)) {
            hideFlyout();
            return;
        }
        const hit = flyout.zoneAtGlobal(p.x, p.y);
        if (hit === null) {
            if (dragHit !== null) {
                dragHit = null;
                flyout.grid.clear();
                zonePreview.visible = false;
            }
            return;
        }
        if (dragHit === null || dragHit.layout !== hit.layout || dragHit.zone !== hit.zone) {
            dragHit = hit;
            flyout.grid.select(hit.layout, hit.zone);
            zonePreview.showAt(previewRect(Layouts.layouts[hit.layout].zones[hit.zone], area));
        }
    }

    // The zone on screen, held 10px off the screen's edges and 4px off the
    // neighbouring zones, as Snap.dc draws it.
    function previewRect(zone: var, area: rect): rect {
        const r = Layouts.zoneRect(zone, area);
        const left = zone[0] < 0.001 ? 10 : 4;
        const top = zone[1] < 0.001 ? 10 : 4;
        const right = zone[0] + zone[2] > 0.999 ? 10 : 4;
        const bottom = zone[1] + zone[3] > 0.999 ? 10 : 4;
        return Qt.rect(r.x + left, r.y + top, r.width - left - right, r.height - top - bottom);
    }

    ZonePreview {
        id: zonePreview
    }

    function trackHover(p: point): void {
        if (flyout.visible && flyout.mode === "hover") {
            if (inside(p, hoverButton, 2) || inside(p, flyout.globalRect(), 8)) {
                leaveTimer.stop();
            } else if (!leaveTimer.running) {
                leaveTimer.start();
            }
            return;
        }
        const window = KWin.Workspace.windowAt(p, 1)[0] ?? null;
        const button = anchorRectFor(window);
        if (button.width > 0 && inside(p, button, 0)) {
            if (hoverWindow !== window || !dwellTimer.running) {
                hoverWindow = window;
                hoverButton = button;
                dwellTimer.restart();
            }
        } else if (dwellTimer.running) {
            dwellTimer.stop();
            hoverWindow = null;
        }
    }

    Timer {
        id: dwellTimer
        interval: 450
        onTriggered: {
            const p = KWin.Workspace.cursorPos;
            if (effect.eligible(effect.hoverWindow) && effect.inside(p, effect.hoverButton, 0)) {
                effect.showHoverFlyout();
            }
        }
    }

    Timer {
        id: leaveTimer
        interval: 300
        onTriggered: effect.hideFlyout()
    }

    Flyout {
        id: flyout
        onDismissed: effect.hideFlyout()
        onZoneChosen: (layoutIndex, zoneIndex) => {
            const window = flyout.target;
            effect.hideFlyout();
            effect.snap(window, layoutIndex, zoneIndex, window ? window.output : null);
        }
    }

    Connections {
        target: KWin.Workspace

        function onCursorPosChanged(): void {
            if (effect.visible) {
                return;
            }
            const p = KWin.Workspace.cursorPos;
            const active = KWin.Workspace.activeWindow;
            if (active && active.move && effect.eligible(active)) {
                if (effect.dragWindow !== active) {
                    effect.hideFlyout();
                    effect.dragWindow = active;
                }
                effect.trackDrag(p);
                return;
            }
            // A drag that ended without KWin reporting it, for instance when
            // the window closed mid-move, must not leave the picker behind.
            if (effect.dragWindow !== null) {
                effect.dragWindow = null;
                effect.hideFlyout();
            }
            effect.trackHover(p);
        }

        function onWindowRemoved(window: var): void {
            if (window === flyout.target || window === effect.hoverWindow) {
                effect.hideFlyout();
                effect.hoverWindow = null;
            }
            if (window === effect.target) {
                effect.close();
            }
        }

        function onWindowActivated(): void {
            if (flyout.visible && flyout.mode === "hover") {
                effect.hideFlyout();
            }
        }

        function onScreensChanged(): void {
            effect.seedTiles();
        }
    }

    // A hover picker belongs to where the button was; if the window moves or
    // changes size (the maximise button was clicked, say) it goes away.
    Connections {
        target: flyout.mode === "hover" ? flyout.target : null

        function onFrameGeometryChanged(): void {
            effect.hideFlyout();
        }
    }

    Connections {
        target: effect.dragWindow

        function onInteractiveMoveResizeFinished(): void {
            const window = effect.dragWindow;
            const hit = effect.dragHit;
            const output = flyout.output;
            effect.dragWindow = null;
            effect.hideFlyout();
            if (hit !== null) {
                effect.snap(window, hit.layout, hit.zone, output);
            }
        }
    }

    // KWin's own default for the tile editor (Meta+T) is three columns at
    // 25/50/25, which suits ultrawide screens but leaves a narrow strip on
    // ordinary ones. Screens that still have that untouched default get two
    // halves instead; anything a person has set up is left alone. Tile
    // layouts are stored per monitor, so this cannot be shipped as config.
    function seedTiles(): void {
        const near = (a, b) => Math.abs(a - b) < 0.01;
        for (const output of KWin.Workspace.screens) {
            const manager = KWin.Workspace.tilingForScreen(output);
            if (!manager || !manager.rootTile) {
                continue;
            }
            const tiles = manager.rootTile.tiles;
            if (tiles.length !== 3 || tiles.some(t => t.tiles.length > 0)) {
                continue;
            }
            const g = tiles.map(t => t.relativeGeometry);
            const untouched = near(g[0].x, 0) && near(g[0].width, 0.25)
                && near(g[1].x, 0.25) && near(g[1].width, 0.5)
                && near(g[2].x, 0.75) && near(g[2].width, 0.25)
                && g.every(r => near(r.y, 0) && near(r.height, 1));
            const geometry = output.geometry;
            if (untouched && geometry.width / geometry.height < 2) {
                tiles[1].remove();
            }
        }
    }

    Component.onCompleted: seedTiles()
}
