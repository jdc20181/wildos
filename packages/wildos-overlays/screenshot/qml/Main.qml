import QtQuick
import QtQuick.Window
import WildOS.Screenshot

// Everything Wild OS Screenshot shows: the frozen overlay on every screen,
// the countdown for a delayed capture, the bar while recording, and the
// Mark up and Share windows the toast's buttons open.
QtObject {
    id: root

    property list<QtObject> parts: [
        Instantiator {
            model: Qt.application.screens
            delegate: Overlay {
                required property var modelData
                screenInfo: modelData
            }
        },
        Countdown {},
        RecordingBar {},
        Connections {
            target: ScreenshotApp
            function onMarkupRequested(file) {
                const window = markupWindow.createObject(root, { file: file });
                window.show();
                window.requestActivate();
            }
            function onShareRequested(file) {
                const window = shareWindow.createObject(root, { file: file });
                window.show();
                window.requestActivate();
            }
        }
    ]

    property Component markupWindow: Component {
        Markup {
            onClosing: {
                ScreenshotApp.windowClosed();
                destroy(1000);
            }
        }
    }

    property Component shareWindow: Component {
        ShareWindow {
            onClosing: {
                ScreenshotApp.windowClosed();
                destroy(1000);
            }
        }
    }
}
