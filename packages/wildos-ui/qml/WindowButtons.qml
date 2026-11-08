import QtQuick
import QtQuick.Window

// The three window buttons in Windows order, close at the far right. They
// act on the window they sit in; set `frame` to the window's WindowFrame
// (AppWindow does) so maximise goes through it, or handle the signals
// yourself by setting `handleActions: false`.
Row {
    id: buttons

    property var frame: null
    property bool maximised: Window.window ? Window.window.visibility === Window.Maximized || Window.window.visibility === Window.FullScreen : false
    property bool windowActive: Window.window ? Window.window.active : true
    property bool showMinimise: true
    property bool showMaximise: true
    property bool handleActions: true
    property real buttonHeight: Style.titleBarHeight
    property real cornerRadius: maximised ? 0 : Style.windowRadius

    signal minimiseClicked()
    signal maximiseClicked()
    signal closeClicked()

    WindowButton {
        visible: buttons.showMinimise
        kind: "minimise"
        height: buttons.buttonHeight
        windowActive: buttons.windowActive
        onClicked: {
            buttons.minimiseClicked();
            if (buttons.handleActions && Window.window) {
                Window.window.showMinimized();
            }
        }
    }

    WindowButton {
        visible: buttons.showMaximise
        kind: buttons.maximised ? "restore" : "maximise"
        height: buttons.buttonHeight
        windowActive: buttons.windowActive
        onClicked: {
            buttons.maximiseClicked();
            if (!buttons.handleActions || !Window.window) {
                return;
            }
            if (buttons.frame) {
                buttons.frame.toggleMaximised();
            } else if (buttons.maximised) {
                Window.window.showNormal();
            } else {
                Window.window.showMaximized();
            }
        }
    }

    WindowButton {
        kind: "close"
        height: buttons.buttonHeight
        cornerRadius: buttons.cornerRadius
        windowActive: buttons.windowActive
        onClicked: {
            buttons.closeClicked();
            if (buttons.handleActions && Window.window) {
                Window.window.close();
            }
        }
    }
}
