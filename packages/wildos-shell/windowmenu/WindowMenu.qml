import QtQuick
import QtQuick.Window
import WildOS.UI as UI

// The window menu (MenusShell.dc "Window title bar") in a clear window over
// the screen, so its submenus have room. The window is a pop-up to KWin:
// it gets the keyboard, and a press outside it closes it.
Window {
    id: root

    property QtObject controller: null

    flags: Qt.Popup | Qt.FramelessWindowHint
    color: "transparent"

    function openMenu(state, px, py) {
        const run = (action, argument) => () => root.controller.act(action, argument);
        const entries = [];
        if (state.canMinimise) {
            entries.push({ text: "Minimise", icon: "minus", run: run("minimise") });
        }
        if (state.canMaximise) {
            entries.push({ text: state.maximised ? "Restore" : "Maximise", icon: state.maximised ? "win-restore" : "menu-maximise", run: run("maximise") });
        }
        if (state.canSnap) {
            entries.push({ text: "Snap", icon: "menu-snap", submenuWidth: 200, submenu: [
                { text: "Left half", run: run("snap", "left") },
                { text: "Right half", run: run("snap", "right") },
                { separator: true },
                { text: "Top left quarter", run: run("snap", "top-left") },
                { text: "Top right quarter", run: run("snap", "top-right") },
                { text: "Bottom left quarter", run: run("snap", "bottom-left") },
                { text: "Bottom right quarter", run: run("snap", "bottom-right") }
            ] });
        }
        if (state.canChangeDesktop) {
            const desktops = state.desktops.map((desktop, index) => ({
                text: desktop.name, checked: desktop.current, run: run("desktop", index)
            }));
            desktops.push({ separator: true }, { text: "New desktop", icon: "plus", run: run("desktop", -1) });
            entries.push({ text: "Move to desktop", icon: "menu-desktop", submenu: desktops, submenuWidth: 200 });
        }
        if (entries.length > 0) {
            entries.push({ separator: true });
        }
        entries.push({ text: "Keep on top", icon: "menu-front", checked: state.keepAbove, run: run("keepAbove") });
        if (state.canMove) {
            entries.push({ text: "Move", icon: "menu-move", run: run("move") });
        }
        if (state.canClose) {
            entries.push({ separator: true }, { text: "Close", icon: "close", shortcut: "Alt+F4", run: run("close") });
        }
        menu.entries = entries;
        visible = true;
        menu.popup(px, py);
    }

    // A press on the clear part of the window is a press outside the menu.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: menu.close()
    }

    UI.Menu {
        id: menu
        minimumWidth: 280
        maximumWidth: 280
        shadow: false
        onClosed: root.visible = false
    }
}
