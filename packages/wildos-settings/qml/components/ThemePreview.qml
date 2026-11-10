pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import WildOS.Settings
import WildOS.UI as UI

// A theme in miniature (SettingsThemes.dc): its wallpaper with a window of
// its glass on it, holding a button and a switch in its accent, and its
// taskbar along the bottom. Drawn from the theme itself, not the current
// look, so every card shows what it will give. The board's card is 250 by
// 124; everything scales with the width.
Item {
    id: preview

    required property var theme
    readonly property bool light: theme.mode === "light"
    readonly property real s: width / 250
    readonly property color ink: light ? Qt.rgba(20 / 255, 26 / 255, 46 / 255, 0.85) : Qt.rgba(238 / 255, 241 / 255, 248 / 255, 0.9)
    readonly property color faint: light ? Qt.rgba(20 / 255, 26 / 255, 46 / 255, 0.14) : Qt.rgba(238 / 255, 241 / 255, 248 / 255, 0.22)
    readonly property color taskbar: theme.taskbarGlass && theme.taskbarGlass.a > 0 ? theme.taskbarGlass
        : light ? Qt.rgba(246 / 255, 248 / 255, 252 / 255, 0.88) : Qt.rgba(14 / 255, 18 / 255, 34 / 255, 0.85)

    implicitWidth: 250
    implicitHeight: 124

    readonly property real radius: 7.2 * s

    // Drawn square, then rounded as a whole by the mask below.
    Rectangle {
        id: scene
        anchors.fill: parent
        color: UI.Style.lakeNavy
        visible: false
        layer.enabled: true

        Image {
            anchors.fill: parent
            source: System.thumbnail(preview.theme.wallpaper)
            fillMode: Image.PreserveAspectCrop
            sourceSize.width: 500
            asynchronous: true
        }

        Rectangle {
            x: 28.7 * preview.s
            y: 22.7 * preview.s
            width: 137.6 * preview.s
            height: 89.7 * preview.s
            radius: 5.4 * preview.s
            color: preview.theme.glass
            border.width: 1
            border.color: preview.faint

            Column {
                x: 7.2 * preview.s
                y: 7.2 * preview.s
                width: parent.width - 2 * x
                spacing: 5.4 * preview.s

                Row {
                    spacing: 2.4 * preview.s
                    Repeater {
                        model: 2
                        Rectangle {
                            width: 3.6 * preview.s
                            height: width
                            radius: width / 2
                            color: preview.faint
                        }
                    }
                }
                Rectangle {
                    width: parent.width * 0.7
                    height: 4.8 * preview.s
                    radius: height / 2
                    color: preview.ink
                    opacity: 0.85
                }
                Rectangle {
                    width: parent.width * 0.9
                    height: 3.6 * preview.s
                    radius: height / 2
                    color: preview.faint
                }
                Rectangle {
                    width: parent.width * 0.76
                    height: 3.6 * preview.s
                    radius: height / 2
                    color: preview.faint
                }
            }

            Rectangle {
                x: 7.2 * preview.s
                y: parent.height - 7.2 * preview.s - height
                width: 41.9 * preview.s
                height: 13.2 * preview.s
                radius: 3.6 * preview.s
                color: preview.theme.accent
            }

            Rectangle {
                x: parent.width - 7.2 * preview.s - width
                y: parent.height - 7.2 * preview.s - 13.2 * preview.s + (13.2 - 10.8) / 2 * preview.s
                width: 20.3 * preview.s
                height: 10.8 * preview.s
                radius: height / 2
                color: preview.theme.accent

                Rectangle {
                    anchors.right: parent.right
                    anchors.rightMargin: 2.4 * preview.s
                    anchors.verticalCenter: parent.verticalCenter
                    width: 6 * preview.s
                    height: width
                    radius: width / 2
                    color: preview.theme.textOnAccent
                }
            }
        }

        Rectangle {
            id: bar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 13.2 * preview.s
            color: preview.taskbar

            Row {
                anchors.centerIn: parent
                spacing: 3.6 * preview.s
                Repeater {
                    model: 6
                    Rectangle {
                        required property int index
                        width: 6 * preview.s
                        height: width
                        radius: 1.8 * preview.s
                        color: index === 2 ? preview.theme.accent : preview.faint
                    }
                }
            }
        }

        // The underglow along the top of the taskbar, for themes that
        // bring it: a bright line with a soft glow either side.
        Rectangle {
            visible: preview.theme.underglow
            anchors.left: parent.left
            anchors.right: parent.right
            y: bar.y - height / 2 - 0.6 * preview.s
            height: 6 * preview.s
            gradient: Gradient {
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 0.5; color: Qt.rgba(preview.theme.accent.r, preview.theme.accent.g, preview.theme.accent.b, 0.45) }
                GradientStop { position: 1; color: "transparent" }
            }
        }
        Rectangle {
            visible: preview.theme.underglow
            anchors.left: parent.left
            anchors.right: parent.right
            y: bar.y - height
            height: Math.max(1, 1.2 * preview.s)
            color: preview.theme.accent
        }
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: preview.radius
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: parent
        source: scene
        maskEnabled: true
        maskSource: mask
    }

    Rectangle {
        anchors.fill: parent
        radius: preview.radius
        color: "transparent"
        border.width: 1
        border.color: UI.Style.borderField
    }
}
