import QtQuick
import WildOS.UI as UI
import WildOS.Screenshot

// The capture toolbar at the top of the screen (Screenshot.dc): 52px glass,
// radius 16, 40 by 36 tools 4px apart in three groups. The shape you pick
// with sits on the accent; Record turns the shapes into what to record;
// Delay waits a few seconds and freezes the screen again, so a menu or a
// hover state can be caught.
Rectangle {
    id: bar

    implicitWidth: tools.implicitWidth + 16
    implicitHeight: 52
    radius: 16
    color: UI.Style.glass(16, 20, 38, 0.84, 0.88)
    border.width: 1
    border.color: UI.Style.borderGlass

    component Tool: UI.IconButton {
        width: 40
        height: 36
        radius: 9
        iconSize: 18
        stroke: 1.9
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
    }

    component Divider: Rectangle {
        width: 13
        height: 26
        color: "transparent"
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined

        Rectangle {
            anchors.centerIn: parent
            width: 1
            height: 26
            color: UI.Style.borderMenu
        }
    }

    Row {
        id: tools
        anchors.centerIn: parent
        height: parent.height
        spacing: 4

        Tool {
            tip: "Rectangle"
            dashedGlyph: "M6 5h12a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V7a2 2 0 0 1 2-2z"
            selected: ScreenshotApp.mode === "rectangle"
            onClicked: ScreenshotApp.mode = "rectangle"
        }

        Tool {
            tip: "Window"
            glyph: "M5 4h14a2 2 0 0 1 2 2v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2zM3 8h18"
            selected: ScreenshotApp.mode === "window"
            onClicked: ScreenshotApp.mode = "window"
        }

        Tool {
            tip: "Full screen"
            glyph: "M4 4h16a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2zM8 21h8"
            selected: ScreenshotApp.mode === "screen"
            onClicked: ScreenshotApp.mode = "screen"
        }

        Tool {
            tip: ScreenshotApp.recordMode ? "Freehand cannot be recorded" : "Freehand"
            dashedGlyph: "M5 15c-2-5 3-11 8-9s7 6 3 9-7 4-11 0z"
            enabled: !ScreenshotApp.recordMode
            selected: ScreenshotApp.mode === "freehand"
            onClicked: ScreenshotApp.mode = "freehand"
        }

        Divider {}

        Tool {
            tip: !ScreenshotApp.canRecord ? "Screen recording is not available in this session"
                : ScreenshotApp.recordMode ? "Record (on): pick what to record" : "Record"
            glyph: "M5 6h9a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2zM16 10l5-3v10l-5-3z"
            enabled: ScreenshotApp.canRecord
            selected: ScreenshotApp.recordMode
            onClicked: ScreenshotApp.recordMode = !ScreenshotApp.recordMode
        }

        Tool {
            id: delayTool
            tip: "Delay"
            glyph: "M5 13a7 7 0 1 0 14 0a7 7 0 1 0-14 0M12 9v4l2 2M9 3h6"
            enabled: !ScreenshotApp.recordMode
            checked: delayMenu.visible
            onClicked: delayMenu.popup(delayTool, 0, delayTool.height + 6)

            UI.Menu {
                id: delayMenu
                minimumWidth: 180

                UI.MenuItem {
                    text: "In 3 seconds"
                    onTriggered: {
                        ScreenshotApp.delay = 3;
                        ScreenshotApp.startCountdown();
                    }
                }
                UI.MenuItem {
                    text: "In 5 seconds"
                    onTriggered: {
                        ScreenshotApp.delay = 5;
                        ScreenshotApp.startCountdown();
                    }
                }
                UI.MenuItem {
                    text: "In 10 seconds"
                    onTriggered: {
                        ScreenshotApp.delay = 10;
                        ScreenshotApp.startCountdown();
                    }
                }
            }
        }

        Divider {}

        Tool {
            tip: "Close (Esc)"
            glyph: "M6 6l12 12M18 6L6 18"
            onClicked: ScreenshotApp.cancel()
        }
    }
}
