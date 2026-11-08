import QtQuick
import org.kde.kwin as KWin

// Windows apps' dialogs (Office's sign-in box, "Save changes?", a file
// dialog) arrive through FreeRDP, which cannot mark them as belonging to
// their app on Wayland (XSetTransientForHint fails under XWayland). Left
// alone they can open behind the app's window, where a modal one blocks the
// app with no way to reach it. A Windows mode window noticeably smaller
// than one already open is taken for a dialog: it is kept above and brought
// forward until it closes. All Windows apps share one FreeRDP process; a
// window's class is FreeRDP's "RAIL:<handle>" until Windows mode tags it
// with its app a moment later.
Item {
    id: root

    function isWindowsApp(window) {
        const cls = String(window && window.resourceClass || "").toLowerCase();
        return window && !window.deleted && (cls.indexOf("wildos-winmode-") === 0 || cls.indexOf("rail:") === 0);
    }

    function area(window) {
        return window.frameGeometry.width * window.frameGeometry.height;
    }

    Connections {
        target: KWin.Workspace

        function onWindowAdded(window) {
            if (!root.isWindowsApp(window)) {
                return;
            }
            let largest = 0;
            for (const other of KWin.Workspace.stackingOrder) {
                if (other !== window && root.isWindowsApp(other) && other.pid === window.pid) {
                    largest = Math.max(largest, root.area(other));
                }
            }
            if (largest > 0 && root.area(window) < largest * 0.6) {
                window.keepAbove = true;
                KWin.Workspace.activeWindow = window;
            }
        }
    }
}
