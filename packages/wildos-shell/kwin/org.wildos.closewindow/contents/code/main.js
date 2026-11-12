// Alt+F4 as Windows has it: it closes the window in use, and on the desktop,
// or with no window in use, it opens the shut down screen (the Wild OS power
// menu, which ksmserver's logout greeter draws from the look and feel).
//
// KWin's own Close Window gives Alt+F4 up to this script. It ignores the
// desktop and the taskbar, so the keys did nothing there.

function closeOrShutDown() {
    const w = workspace.activeWindow;
    if (!w || w.desktopWindow || w.dock) {
        callDBus("org.kde.LogoutPrompt", "/LogoutPrompt", "org.kde.LogoutPrompt", "promptAll");
        return;
    }
    if (w.closeable) {
        w.closeWindow();
    }
}

registerShortcut("Wild OS close window", "Close the window, or shut down from the desktop", "Alt+F4", closeOrShutDown);
