pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import org.kde.taskmanager as TaskManager
import WildOS.UI as UI
import org.wildos.overlays
import WildOS.Clipboard

// The clipboard and emoji panel (ClipboardEmoji.dc): 380 wide, radius 16,
// glass, 12px inside, with Emoji, Symbols and Clipboard as pill tabs. It
// opens in the bottom right corner above the taskbar, where the flyouts
// open, and closes on Escape, on the same shortcut again, or when another
// window is clicked.
Window {
    id: panel

    readonly property bool clipboardTab: PanelApp.tab === "clipboard"
    property bool wasActive: false

    // The app in front, for "· Files" under what it copied and to paste
    // into terminals with Ctrl+Shift+V.
    readonly property string activeApp: tasks.activeTask.valid
        ? String(tasks.data(tasks.activeTask, TaskManager.AbstractTasksModel.AppName) || "") : ""
    readonly property bool activeIsTerminal: {
        if (!tasks.activeTask.valid) {
            return false;
        }
        const id = String(tasks.data(tasks.activeTask, TaskManager.AbstractTasksModel.AppId) || "").toLowerCase();
        return id.indexOf("terminal") >= 0 || id.indexOf("konsole") >= 0 || id.indexOf("alacritty") >= 0 || id.indexOf("kitty") >= 0;
    }

    width: 380
    height: clipboardTab ? 560 : 460
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | (placement.wayland ? 0 : Qt.Tool)
    title: "Clipboard and emoji"
    visible: false

    TaskManager.TasksModel {
        id: tasks
        groupMode: TaskManager.TasksModel.GroupDisabled
    }

    OverlayPlacement {
        id: placement
        window: panel
        edges: OverlayPlacement.Bottom | OverlayPlacement.Right
        marginRight: 16
        // 14px above the taskbar pills, as the flyouts sit.
        marginBottom: 90
        layer: OverlayPlacement.Overlay
        keyboard: OverlayPlacement.OnDemand
        slideFrom: UI.Style.reducedMotion ? OverlayPlacement.NoEdge : OverlayPlacement.Bottom
        scope: "wildos-clipboard"
    }

    UI.WindowFrame {
        window: panel
        radius: 16
        shadowColor: UI.Style.shadowFlyout
    }

    Connections {
        target: PanelApp
        function onShownChanged() {
            if (PanelApp.shown) {
                panel.wasActive = false;
                panel.visible = true;
                panel.requestActivate();
                pages.focusCurrent();
            } else {
                panel.visible = false;
            }
        }
    }

    onActiveChanged: {
        if (active) {
            wasActive = true;
        } else if (wasActive && visible) {
            PanelApp.shown = false;
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 16
        color: UI.Style.glass(16, 20, 38, 0.84, 0.88)
        border.width: 1
        border.color: UI.Style.borderGlass
    }

    Item {
        id: pages
        anchors.fill: parent
        anchors.margins: 12
        focus: true

        function focusCurrent(): void {
            if (PanelApp.tab === "clipboard") {
                clipboardPage.focusList();
            } else if (PanelApp.tab === "emoji") {
                emojiPage.focusSearch();
            } else {
                symbolsPage.focusSearch();
            }
        }

        Keys.onEscapePressed: PanelApp.shown = false

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Row {
                id: tabs
                Layout.fillWidth: true
                spacing: 4

                UI.SegmentButton {
                    text: "Emoji"
                    checked: PanelApp.tab === "emoji"
                    onClicked: {
                        PanelApp.tab = "emoji";
                        pages.focusCurrent();
                    }
                }
                UI.SegmentButton {
                    text: "Symbols"
                    checked: PanelApp.tab === "symbols"
                    onClicked: {
                        PanelApp.tab = "symbols";
                        pages.focusCurrent();
                    }
                }
                UI.SegmentButton {
                    text: "Clipboard"
                    checked: PanelApp.tab === "clipboard"
                    onClicked: {
                        PanelApp.tab = "clipboard";
                        pages.focusCurrent();
                    }
                }
            }

            ClipboardPage {
                id: clipboardPage
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: PanelApp.tab === "clipboard"
                activeApp: panel.activeApp
                terminal: panel.activeIsTerminal
            }

            GridPage {
                id: emojiPage
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: PanelApp.tab === "emoji"
                emoji: true
                terminal: panel.activeIsTerminal
            }

            GridPage {
                id: symbolsPage
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: PanelApp.tab === "symbols"
                emoji: false
                terminal: panel.activeIsTerminal
            }
        }
    }
}
