import QtQuick
import QtQuick.Window
import WildOS.Files
import WildOS.UI as UI

// A Files window. It has no system title bar: the tabs live in its own,
// so the Wild OS app window supplies the glass, shadow, blur and resize
// grips the Aurorae decoration would otherwise give it.
UI.AppWindow {
    id: window

    property var startUrls: []
    property string formatUdi: ""

    width: Settings.windowSize().width
    height: Settings.windowSize().height
    visible: true
    title: view.title

    onActiveChanged: if (active) FileOps.activeWindow = window
    onClosing: {
        if (!maximised) {
            Settings.setWindowSize(Qt.size(width, height));
        }
    }
    Component.onCompleted: FileOps.activeWindow = window

    FilesView {
        id: view
        anchors.fill: parent
        anchors.margins: window.maximised ? 0 : 1
        startUrls: window.startUrls
        formatUdi: window.formatUdi
        frame: window.frame
        onCloseWindow: window.close()
    }
}
