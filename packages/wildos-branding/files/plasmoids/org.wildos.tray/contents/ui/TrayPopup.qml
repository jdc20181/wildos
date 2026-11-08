pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI
import org.kde.ksvg as KSvg
import org.kde.plasma.private.devicenotifier as DeviceNotifier

// The popup above the tray, in one of two forms, as on Windows 11: a small
// grid of the hidden app icons (at most four across, no heading, no labels),
// or a short list of drives to eject. Plasma's theme draws the glass, so this
// only paints what sits on it.
FocusScope {
    id: popup

    required property var tray

    KSvg.FrameSvgItem {
        id: frame
        imagePath: "dialogs/background"
        visible: false
    }

    // 8 px of glass around the content, inside the 1px border.
    readonly property real inset: 1 + 8
    readonly property real cell: 40
    readonly property int columns: Math.max(1, Math.min(4, tray.overflowCount))
    readonly property int rows: Math.ceil(tray.overflowCount / columns)
    readonly property bool usb: tray.mode === "usb"

    readonly property real glassWidth: usb ? 300 : 2 * inset + columns * cell + (columns - 1) * 2
    readonly property real glassHeight: usb ? 2 * inset + drives.implicitHeight
        : 2 * inset + rows * cell + (rows - 1) * 2
    readonly property real contentWidth: Math.max(1, glassWidth - frame.margins.left - frame.margins.right)
    readonly property real contentHeight: Math.max(1, glassHeight - frame.margins.top - frame.margins.bottom)

    Layout.minimumWidth: contentWidth
    Layout.preferredWidth: contentWidth
    Layout.maximumWidth: contentWidth
    Layout.minimumHeight: contentHeight
    Layout.preferredHeight: contentHeight
    Layout.maximumHeight: contentHeight

    focus: true

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Escape) {
            tray.expanded = false;
            event.accepted = true;
            return;
        }
        if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) {
            Nav.keyboard = true;
        }
        event.accepted = false;
    }

    HoverHandler {
        onPointChanged: Nav.keyboard = false
    }

    Connections {
        target: popup.tray
        function onExpandedChanged() {
            if (popup.tray.expanded) {
                Nav.keyboard = false;
                popup.forceActiveFocus();
            }
        }
    }

    GridLayout {
        id: grid
        visible: !popup.usb
        x: popup.inset - frame.margins.left
        y: popup.inset - frame.margins.top
        columns: popup.columns
        columnSpacing: 2
        rowSpacing: 2

        Repeater {
            model: popup.tray.items

            TrayIcon {
                required property int index
                visible: popup.tray.inlineRows.indexOf(index) < 0
                Layout.preferredWidth: popup.cell
                Layout.preferredHeight: popup.cell
                onUsed: popup.tray.expanded = false
            }
        }
    }

    ColumnLayout {
        id: drives
        visible: popup.usb
        x: popup.inset - frame.margins.left
        y: popup.inset - frame.margins.top
        width: 300 - 2 * popup.inset
        spacing: 2

        Repeater {
            model: popup.tray.mountedDrives

            MouseArea {
                id: drive

                required property string deviceDescription
                required property int deviceOperationResult
                required property var deviceActions

                readonly property bool busy: deviceOperationResult === DeviceNotifier.DevicesStateMonitor.Working
                readonly property bool failed: deviceOperationResult === DeviceNotifier.DevicesStateMonitor.Unsuccessful
                readonly property bool showFocus: activeFocus && Nav.keyboard

                function eject(): void {
                    if (!busy && deviceActions) {
                        deviceActions.actionTriggered(deviceActions.defaultActionName);
                    }
                }

                Layout.fillWidth: true
                Layout.preferredHeight: 40 + (failed ? note.implicitHeight + 6 : 0)
                hoverEnabled: true
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: label.text
                Accessible.onPressAction: eject()
                Keys.onReturnPressed: eject()
                Keys.onEnterPressed: eject()
                Keys.onSpacePressed: eject()
                onClicked: eject()

                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: drive.pressed ? UI.Style.surfacePressed : drive.containsMouse ? UI.Style.surfaceHover : "transparent"

                    UI.FocusRing {
                        visible: drive.showFocus
                        targetRadius: 8
                    }
                }

                UI.Glyph {
                    x: 12
                    y: 12
                    width: 16
                    height: 16
                    path: "M8 3h6v6H8zM6 9h10v6a3 3 0 0 1-3 3h-4a3 3 0 0 1-3-3zM11 18v3"
                }

                UI.Label {
                    id: label
                    x: 40
                    y: 0
                    height: 40
                    width: parent.width - 52
                    text: drive.busy ? "Ejecting " + drive.deviceDescription : "Eject " + drive.deviceDescription
                    color: drive.busy ? UI.Style.inkMuted : UI.Style.ink
                }

                // Unmounting fails while a file on the drive is still open.
                UI.Label {
                    id: note
                    visible: drive.failed
                    x: 40
                    y: 36
                    width: parent.width - 52
                    text: "Something is still using this drive. Close any files on it, then try again."
                    color: UI.Style.inkMuted
                    size: 12
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                }
            }
        }
    }
}
