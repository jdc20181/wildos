pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.kde.ksvg as KSvg
import WildOS.UI as UI

// The panel from board 4.14. While Windows apps are open: the edition, a
// Running pill, processor use over the last minute, memory against the limit
// set for Windows, what is open, and Pause now and Save to disk along a
// darker foot. While paused (and once saved or off) it is narrower: how long,
// the memory held, and the app to open again. It only offers what the person
// can act on, so Save to disk waits until the service can save, and an app
// is only offered once one has been opened. Plasma's theme draws the glass;
// sizes are the board's outer edge with the theme's frame margins taken off,
// as in the security flyout.
FocusScope {
    id: panel

    required property var winmode

    KSvg.FrameSvgItem {
        id: frame
        imagePath: "dialogs/background"
        visible: false
    }

    readonly property real edgeLeft: frame.margins.left
    readonly property real edgeRight: frame.margins.right
    readonly property real edgeTop: frame.margins.top
    readonly property real edgeBottom: frame.margins.bottom

    readonly property string level: winmode.level
    readonly property bool running: level === "running"
    readonly property bool resting: level === "paused" || level === "saved" || level === "off"
    readonly property bool vmRunning: winmode.system !== null && winmode.system.vm === "running"

    readonly property int glassBorder: 1
    readonly property int pad: 18
    readonly property int frameRadius: 22
    readonly property int outerWidth: running ? 360 : 330
    readonly property real innerWidth: outerWidth - 2 * (glassBorder + pad)

    readonly property bool hasFoot: !resting
    readonly property real footHeight: hasFoot ? foot.implicitHeight + 26 : 0
    readonly property real glassHeight: 2 * glassBorder + pad + body.implicitHeight + (hasFoot ? 14 + footHeight : pad)

    readonly property real contentWidth: outerWidth - edgeLeft - edgeRight
    readonly property real contentHeight: glassHeight - edgeTop - edgeBottom

    Layout.minimumWidth: contentWidth
    Layout.preferredWidth: contentWidth
    Layout.maximumWidth: contentWidth
    Layout.minimumHeight: contentHeight
    Layout.preferredHeight: contentHeight
    Layout.maximumHeight: contentHeight

    focus: true

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Escape) {
            panel.winmode.expanded = false;
            event.accepted = true;
        }
    }

    Connections {
        target: panel.winmode
        function onExpandedChanged() {
            if (panel.winmode.expanded) {
                panel.forceActiveFocus();
            }
        }
    }

    component Caption: UI.Label {
        size: 12
        color: UI.Style.inkMuted
    }

    component Figure: UI.Label {
        size: 13
        font.weight: Font.DemiBold
        font.features: ({ "tnum": 1 })
    }

    component Note: UI.Label {
        Layout.fillWidth: true
        size: 11
        color: UI.Style.inkMuted
        wrapMode: Text.Wrap
        elide: Text.ElideNone
    }

    component Action: UI.Button {
        Layout.fillWidth: true
        Layout.preferredHeight: 34
    }

    // "Windows mode settings" and a chevron across the panel's width. Inline
    // components can't reach the panel's ids, so each use says what a click
    // does.
    component SettingsLink: T.AbstractButton {
        id: link
        Layout.fillWidth: true
        implicitHeight: 20
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        text: "Windows mode settings"
        Accessible.role: Accessible.Link
        Accessible.name: text

        background: Item {
            UI.FocusRing {
                visible: link.visualFocus
                targetRadius: UI.Style.radiusXs
            }
        }

        contentItem: RowLayout {
            spacing: 8

            UI.Label {
                Layout.fillWidth: true
                text: link.text
                size: 12
                color: UI.Style.accentText
                font.underline: link.hovered
                opacity: link.down ? 0.75 : 1
            }

            UI.Glyph {
                Layout.preferredWidth: 12
                Layout.preferredHeight: 12
                path: "M9 6l6 6-6 6"
                color: UI.Style.accentText
                strokeWidth: 2
            }
        }
    }

    ColumnLayout {
        id: body
        x: panel.glassBorder + panel.pad - panel.edgeLeft
        y: panel.glassBorder + panel.pad - panel.edgeTop
        width: panel.innerWidth
        spacing: panel.running ? 16 : 14

        // The mark in an accent tile, "Windows mode" and what it is doing,
        // and the state in a pill.
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                radius: 10
                color: UI.Style.accentChip

                UI.Icon {
                    anchors.centerIn: parent
                    name: "windowsapps"
                    size: 20
                    strokeWidth: 1.8
                    color: UI.Style.accentText
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                UI.Label {
                    Layout.fillWidth: true
                    text: "Windows mode"
                    size: 15
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                }

                Caption {
                    Layout.fillWidth: true
                    text: panel.winmode.subtitle()
                }
            }

            Rectangle {
                Layout.preferredHeight: 22
                Layout.preferredWidth: pill.implicitWidth + 18
                radius: 11
                color: Tones.pillFill(panel.level)

                RowLayout {
                    id: pill
                    anchors.centerIn: parent
                    spacing: 6

                    StatusDot {
                        level: panel.level
                    }

                    UI.Label {
                        text: panel.winmode.pillText
                        size: 11
                        font.weight: Font.DemiBold
                        color: Tones.pillText(panel.level)
                    }
                }
            }
        }

        ColumnLayout {
            visible: panel.running && panel.winmode.qemu !== null
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true

                Caption {
                    Layout.fillWidth: true
                    text: "Processor, last minute"
                }

                Figure {
                    text: panel.winmode.processorPercent + "%"
                }
            }

            Sparkline {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                points: panel.winmode.processorTrace
            }
        }

        ColumnLayout {
            visible: (panel.running || panel.level === "paused") && panel.winmode.qemu !== null
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true

                Caption {
                    Layout.fillWidth: true
                    text: panel.running ? "Memory" : "Memory held"
                }

                Figure {
                    text: panel.running && panel.winmode.memoryLimit > 0
                        ? panel.winmode.bytesText(panel.winmode.memoryHeld) + " of " + panel.winmode.bytesText(panel.winmode.memoryLimit)
                        : panel.winmode.bytesText(panel.winmode.memoryHeld)
                }
            }

            Meter {
                Layout.fillWidth: true
                value: panel.winmode.memoryLimit > 0 ? panel.winmode.memoryHeld / panel.winmode.memoryLimit : 0
                held: !panel.running
                Accessible.name: panel.running ? "Memory" : "Memory held"
            }

            Note {
                text: panel.running ? "Unused memory goes back to Wild OS" : "No processor use. Ready in about a second."
            }
        }

        // Saved or off: nothing held, so no meter, only what it costs to
        // come back.
        UI.Label {
            visible: panel.level === "saved" || panel.level === "off"
            Layout.fillWidth: true
            size: 12
            color: UI.Style.inkMuted
            lineHeight: 1.5
            wrapMode: Text.Wrap
            elide: Text.ElideNone
            text: panel.level === "saved" ? "Uses no memory. Ready in a few seconds when you open a Windows app."
                : "Uses no memory. Starts when you open a Windows app, which takes about a minute."
        }

        UI.Label {
            visible: panel.level === "starting" || panel.level === "updating" || panel.level === "attention"
            Layout.fillWidth: true
            size: 13
            lineHeight: 1.5
            wrapMode: Text.Wrap
            elide: Text.ElideNone
            color: panel.level === "attention" ? UI.Style.ink : UI.Style.inkMuted
            text: panel.level === "starting" ? "Getting Windows ready. Your app opens as soon as it is."
                : panel.level === "updating" ? "Installing Windows and Office updates that Wild OS has tested. Your apps open as normal once it's done."
                : panel.winmode.attentionText()
        }

        ColumnLayout {
            visible: panel.running && panel.winmode.openApps.length > 0
            Layout.fillWidth: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.bottomMargin: 8
                implicitHeight: 1
                color: UI.Style.tone(0.08)
            }

            Caption {
                Layout.bottomMargin: 2
                text: "Open now"
            }

            Repeater {
                model: panel.winmode.openApps

                RowLayout {
                    id: app
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        radius: 8
                        color: UI.Style.tone(0.08)

                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: 20
                            height: 20
                            source: app.modelData.icon || "wildos-windows-app"
                            fallback: "wildos-windows-app"
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        UI.Label {
                            Layout.fillWidth: true
                            text: app.modelData.name
                            size: 13
                        }

                        UI.Label {
                            visible: text.length > 0
                            Layout.fillWidth: true
                            text: app.modelData.title || ""
                            size: 11
                            color: UI.Style.inkMuted
                        }
                    }
                }
            }
        }

        UI.Label {
            readonly property string idle: panel.running ? panel.winmode.idleText() : ""
            visible: idle.length > 0
            Layout.fillWidth: true
            size: 12
            color: UI.Style.inkMuted
            lineHeight: 1.5
            wrapMode: Text.Wrap
            elide: Text.ElideNone
            text: idle
        }

        // Resting: Save to disk while it can still free memory, and the app
        // to open again as the main action. With neither, the way to the
        // settings.
        RowLayout {
            id: restingActions
            readonly property bool canSave: panel.level === "paused" && panel.winmode.canSave
            readonly property bool canOpen: panel.winmode.lastApp !== null
            visible: panel.resting && (canSave || canOpen)
            Layout.fillWidth: true
            spacing: 8

            Action {
                visible: restingActions.canSave
                enabled: !panel.winmode.busyAction
                text: "Save to disk"
                onClicked: panel.winmode.saveToDisk()
            }

            Action {
                visible: restingActions.canOpen
                kind: "primary"
                text: restingActions.canOpen ? "Open " + panel.winmode.lastApp.name : ""
                onClicked: panel.winmode.openApp(panel.winmode.lastApp.id)
            }
        }

        SettingsLink {
            visible: panel.resting && !restingActions.visible
            onClicked: panel.winmode.openSettings()
        }
    }

    // The darker foot, edge to edge inside the glass border, as on quick
    // settings: Pause now and Save to disk while Windows runs, and the way
    // to Windows mode's settings.
    Rectangle {
        visible: panel.hasFoot
        anchors.left: parent.left
        anchors.leftMargin: panel.glassBorder - panel.edgeLeft
        anchors.right: parent.right
        anchors.rightMargin: panel.glassBorder - panel.edgeRight
        anchors.bottom: parent.bottom
        anchors.bottomMargin: panel.glassBorder - panel.edgeBottom
        height: panel.footHeight
        color: UI.Style.surfaceBand
        bottomLeftRadius: panel.frameRadius - panel.glassBorder
        bottomRightRadius: panel.frameRadius - panel.glassBorder

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: UI.Style.tone(0.07)
        }

        ColumnLayout {
            id: foot
            x: panel.pad
            y: 12
            width: panel.innerWidth
            spacing: 10

            RowLayout {
                visible: panel.vmRunning && (panel.running || panel.level === "attention")
                Layout.fillWidth: true
                spacing: 8

                Action {
                    enabled: !panel.winmode.busyAction
                    text: "Pause now"
                    onClicked: panel.winmode.pauseNow()
                }

                Action {
                    visible: panel.winmode.canSave
                    enabled: !panel.winmode.busyAction
                    text: "Save to disk"
                    onClicked: panel.winmode.saveToDisk()
                }
            }

            SettingsLink {
                onClicked: panel.winmode.openSettings()
            }
        }
    }
}
