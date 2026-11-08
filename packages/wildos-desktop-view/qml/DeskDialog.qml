import QtQuick
import WildOS.UI as UI

// A WildOS.UI dialog filling its DialogHost window: the window is the
// dialog, so there is no scrim and no margin round it.
UI.Dialog {
    id: control

    property var host: null
    // What Enter does, as the default button does on Windows.
    property var defaultAction: null

    modal: false
    dim: false
    width: parent ? parent.width : 440

    function present() {
        if (host) {
            host.present();
        } else {
            open();
        }
    }

    // Inside the dialog itself: Qt Quick only lets an open popup's own
    // shortcuts through.
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: control.visible && typeof control.defaultAction === "function"
        onActivated: control.defaultAction()
    }
}
