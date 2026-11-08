import QtQuick
import org.kde.kwin as KWin

// Tells wildos-launchd about every app window that opens or closes. The
// daemon uses it to know when a launch it is following has its window (the
// taskbar's starting line ends then) and which process to sample for
// preloading.
Item {
    id: root

    function keysOf(window) {
        const keys = [];
        const add = value => {
            const key = String(value || "").toLowerCase();
            if (key.length > 0 && keys.indexOf(key) < 0) {
                keys.push(key);
            }
        };
        add(window.desktopFileName);
        add(window.resourceClass);
        return keys;
    }

    function isAppWindow(window) {
        return window && !window.deleted && window.normalWindow && !window.specialWindow && !window.transient
            && !window.skipTaskbar;
    }

    Component {
        id: callComponent
        KWin.DBusCall {
            service: "org.wildos.Launch"
            path: "/org/wildos/Launch"
            dbusInterface: "org.wildos.Launch"
            onFinished: destroy()
            onFailed: destroy()
        }
    }

    // The app windows open now. Kept here because by the time KWin says a
    // window was removed it is already marked deleted.
    property var tracked: []

    function report(added) {
        const open = tracked.map(window => ({ k: keysOf(window), pid: window.pid }));
        const c = callComponent.createObject(root, {
            method: "Windows",
            arguments: [JSON.stringify({ open: open, added: added ? { k: keysOf(added), pid: added.pid } : null })]
        });
        c.call();
    }

    Connections {
        target: KWin.Workspace

        function onWindowAdded(window) {
            if (root.isAppWindow(window)) {
                root.tracked.push(window);
                root.report(window);
            }
        }

        function onWindowRemoved(window) {
            const index = root.tracked.indexOf(window);
            if (index >= 0) {
                root.tracked.splice(index, 1);
                root.report(null);
            }
        }
    }

    Component.onCompleted: {
        for (const window of KWin.Workspace.stackingOrder) {
            if (isAppWindow(window)) {
                tracked.push(window);
            }
        }
        report(null);
    }
}
