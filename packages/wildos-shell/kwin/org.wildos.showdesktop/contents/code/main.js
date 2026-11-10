// Show desktop as Windows does it. Windows key + D, or a click on the corner
// of the taskbar, minimises every window into the taskbar with the normal
// minimise animation. Doing it again brings back exactly those windows, in
// the order they were stacked, with the one that was in use in front.
// Opening or restoring a window in between starts afresh, so the next press
// minimises again.
//
// KWin's own Show Desktop moves the windows aside without minimising them,
// which reads as the windows flying off the screen. It keeps its action for
// the taskbar's peek, which fades the windows instead (org.wildos.peekfade).

const shortcut = "Wild OS show desktop";

// The windows this script minimised, bottom of the stack first.
let saved = [];
let savedActive = null;
// Set while this script minimises or restores, so its own changes do not
// count as the person restoring a window.
let busy = false;

function forget() {
    saved = [];
    savedActive = null;
}

function onCurrentDesktop(w) {
    return w.onAllDesktops || w.desktops.indexOf(workspace.currentDesktop) !== -1;
}

// Only windows with a taskbar button, so everything minimised can be found
// again. Dialogs follow their main window.
function minimisable(w) {
    return w.normalWindow && w.minimizable && !w.minimized && !w.skipTaskbar
        && !w.transient && onCurrentDesktop(w);
}

function watch(w) {
    w.minimizedChanged.connect(() => {
        if (!busy && !w.minimized) {
            forget();
        }
    });
}

function minimiseAll() {
    const windows = workspace.stackingOrder.filter(minimisable);
    if (windows.length === 0) {
        return;
    }
    const active = workspace.activeWindow;
    busy = true;
    for (let i = windows.length - 1; i >= 0; i--) {
        windows[i].minimized = true;
    }
    busy = false;
    saved = windows;
    savedActive = windows.indexOf(active) !== -1 ? active : windows[windows.length - 1];
}

function restore() {
    const windows = saved.filter(w => w.minimized);
    const active = savedActive;
    forget();
    busy = true;
    for (const w of windows) {
        w.minimized = false;
    }
    for (const w of windows) {
        workspace.raiseWindow(w);
    }
    busy = false;
    if (active && windows.indexOf(active) !== -1) {
        workspace.activeWindow = active;
    }
}

function toggle() {
    if (saved.length > 0 && !workspace.stackingOrder.some(minimisable)) {
        restore();
    } else {
        minimiseAll();
    }
}

workspace.windowList().forEach(watch);
workspace.windowAdded.connect(w => {
    watch(w);
    if (w.normalWindow && !w.skipTaskbar) {
        forget();
    }
});
workspace.windowRemoved.connect(w => {
    saved = saved.filter(s => s !== w);
    if (savedActive === w) {
        savedActive = null;
    }
});
workspace.currentDesktopChanged.connect(forget);

registerShortcut(shortcut, "Show the desktop (minimise every window)", "Meta+D", toggle);
