// Windows key + backtick steps through the current app's windows, as Cmd+`
// does on a Mac. KWin's own "Walk Through Windows of Current Application"
// keeps Alt+`, but it orders windows by when they were last used, so with
// three or more windows it flips between the same two. This walks them in
// the order they were opened, so every window comes round in turn.

function appWindows(active) {
    const desktop = workspace.currentDesktop;
    return workspace.windowList().filter(w =>
        w.normalWindow && !w.skipSwitcher && w.resourceClass === active.resourceClass
        && (w.onAllDesktops || w.desktops.indexOf(desktop) !== -1));
}

function step(by) {
    const active = workspace.activeWindow;
    if (!active) {
        return;
    }
    const windows = appWindows(active);
    if (windows.length < 2) {
        return;
    }
    const next = windows[(windows.indexOf(active) + by + windows.length) % windows.length];
    next.minimized = false;
    workspace.activeWindow = next;
}

registerShortcut("Wild OS next window of this app", "Next window of the same app", "Meta+`", () => step(1));
