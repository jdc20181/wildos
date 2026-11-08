import QtQuick
import QtQuick.Window
import WildOS.UI as UI

// A window of its own for a desktop dialog. The desktop sits under every
// other window, so a dialog drawn inside it would be hidden whenever an app
// covers the screen; on Windows these questions come up as windows too.
// The window is exactly the dialog's size, and the compositor draws its
// shadow and blur through WindowFrame.
Window {
    id: host

    property var dialog: null

    flags: Qt.Dialog | Qt.FramelessWindowHint
    color: "transparent"
    width: 440
    height: dialog ? Math.max(120, Math.ceil(dialog.implicitHeight)) : 200
    title: dialog ? dialog.title : ""

    function present() {
        if (!dialog) {
            return;
        }
        if (transientParent && transientParent.screen) {
            screen = transientParent.screen;
            x = screen.virtualX + Math.round((screen.width - width) / 2);
            y = screen.virtualY + Math.round((screen.height - height) / 2.4);
        }
        show();
        raise();
        DeskOps.bringForward(host, transientParent);
        dialog.open();
    }

    UI.WindowFrame {
        window: host
        radius: UI.Style.radiusDialog
        shadowColor: UI.Style.shadowMenu
        blur: true
    }

    Connections {
        target: host.dialog
        function onClosed() {
            host.hide();
        }
    }

    onClosing: close => {
        if (dialog && dialog.visible) {
            dialog.close();
        }
    }
}
