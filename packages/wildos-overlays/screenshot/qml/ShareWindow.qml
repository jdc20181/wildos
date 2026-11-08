pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import WildOS.UI as UI
import WildOS.Screenshot

// Share, from the screenshot or recording toast: the picture and the ways
// this computer can send it (email, Bluetooth, an image host), as rows.
UI.AppWindow {
    id: win

    required property url file
    readonly property var targets: ScreenshotApp.sharer.targets(file)
    property bool busy: false

    deep: true
    title: "Share"
    width: 400
    height: Math.min(560, 42 + 180 + 16 + Math.max(1, targets.length) * 49 + 72)
    minimumWidth: 360
    minimumHeight: 320

    Connections {
        target: ScreenshotApp
        function onMessageChanged() {
            win.busy = false;
            if (ScreenshotApp.message.length === 0) {
                win.close();
            }
        }
    }

    Shortcut { sequence: "Esc"; onActivated: win.close() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        UI.TitleBar {
            Layout.fillWidth: true
            frame: win.frame
            title: win.title
            buttons.showMinimise: false
            buttons.showMaximise: false
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            radius: UI.Style.radiusMd
            color: UI.Style.surfaceSunken
            clip: true

            Image {
                anchors.fill: parent
                anchors.margins: 8
                source: String(win.file).match(/\.png$/) ? win.file : ""
                fillMode: Image.PreserveAspectFit
                sourceSize.width: 720
                smooth: true
            }

            UI.Icon {
                anchors.centerIn: parent
                visible: !String(win.file).match(/\.png$/)
                name: "video"
                size: 40
                color: UI.Style.iconMuted
            }
        }

        UI.Label {
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            visible: ScreenshotApp.message.length > 0
            text: ScreenshotApp.message
            wrapMode: Text.Wrap
            color: UI.Style.inkStatus
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 12
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 12
            clip: true
            model: win.targets
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: UI.ScrollBar {}

            delegate: UI.ListRow {
                required property var modelData
                width: ListView.view.width
                themeIcon: modelData.icon
                title: modelData.name
                interactive: !win.busy
                radius: UI.Style.radiusSm
                divider: false
                onClicked: {
                    win.busy = true;
                    ScreenshotApp.sharer.share(modelData.row);
                }
            }

            // Nothing on this computer can share this kind of file.
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 32
                visible: list.count === 0
                spacing: 6

                UI.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: "No ways to share this yet"
                    font.weight: Font.DemiBold
                }
                UI.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    size: 12
                    color: UI.Style.inkMuted
                    text: "Add an email account or turn on Bluetooth to send it from here. It is already on the clipboard to paste anywhere."
                }
                UI.Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 8
                    text: "Show in folder"
                    onClicked: {
                        ScreenshotApp.showInFolder(win.file);
                        win.close();
                    }
                }
            }
        }
    }
}
