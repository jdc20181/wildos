import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Going back to a restore point (RecoveryGoBack.dc): what goes back, what
// stays as it is, holding the update back, and Go back and restart. Opened
// from System > Recovery and from Updates' "Go back" on the latest Wild OS
// build.
RecoveryDialog {
    id: dialog

    property var point: ({})
    readonly property string label: point.label || ""
    readonly property bool before: label.startsWith("Before ")
    readonly property var apps: point.apps || []
    readonly property bool canHold: point.holdable === true && (point.build || 0) > 0
    property bool hold: true

    function openFor(id) {
        point = Recovery.point(id);
        hold = true;
        open();
    }

    function lowerFirst(text) {
        return text.length > 0 ? text.charAt(0).toLowerCase() + text.slice(1) : text;
    }

    function whenText(date) {
        if (!Format.isValid(date)) {
            return "";
        }
        const day = Format.day(date);
        return (day === "today" || day === "yesterday" ? day : "on " + day) + " at " + Format.time(date);
    }

    function goesBackSystem() {
        if (point.system && point.driver) {
            return "System files and the graphics driver";
        }
        if (point.driver) {
            return "The graphics driver";
        }
        if (point.system || apps.length === 0) {
            return apps.length === 0 && !point.system ? "System files and apps, as they were then" : "System files";
        }
        return "";
    }

    function goesBackApps() {
        if (apps.length === 0) {
            return "";
        }
        const shown = apps.slice(0, 3);
        const more = apps.length - shown.length;
        const names = more > 0 ? shown.join(", ") + " and " + Format.plural(more, "more app", "more apps")
            : shown.length === 1 ? shown[0] : shown.slice(0, -1).join(", ") + " and " + shown[shown.length - 1];
        return names + ", to " + (apps.length === 1 ? "its earlier version" : "their earlier versions");
    }

    title: before ? "Go back to " + lowerFirst(label) + "?" : "Go back to this restore point?"
    dialogWidth: 560

    UI.Label {
        Layout.fillWidth: true
        text: "The PC goes back to how it was " + dialog.whenText(dialog.point.when)
            + (dialog.before ? ", just " + dialog.lowerFirst(dialog.label) : "")
            + (Recovery.backend === "btrfs" ? ". It takes about a minute and restarts once." : ". It takes a few minutes and restarts once.")
        size: 13.5
        lineHeight: 1.5
        wrapMode: Text.Wrap
        elide: Text.ElideNone
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 20

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            spacing: 10

            UI.Label {
                text: "GOES BACK"
                size: 12.5
                font.weight: UI.Style.weightSemiBold
                color: UI.Style.inkSubtle
            }

            Repeater {
                model: [dialog.goesBackSystem(), dialog.goesBackApps()].filter(t => t.length > 0)

                RowLayout {
                    required property string modelData
                    Layout.fillWidth: true
                    spacing: 9

                    PathIcon {
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 1
                        name: "undo-2"
                        size: 15
                        strokeWidth: 2.2
                        color: "#ffc46a"
                    }

                    UI.Label {
                        Layout.fillWidth: true
                        text: modelData
                        size: 13
                        lineHeight: 1.4
                        wrapMode: Text.Wrap
                        elide: Text.ElideNone
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            spacing: 10

            UI.Label {
                text: "STAYS AS IT IS"
                size: 12.5
                font.weight: UI.Style.weightSemiBold
                color: UI.Style.inkSubtle
            }

            Repeater {
                model: ["Your files, Mail, Calendar and Notes", "Your settings, passwords and Wi-Fi", "Windows mode and its apps"]

                RowLayout {
                    required property string modelData
                    Layout.fillWidth: true
                    spacing: 9

                    PathIcon {
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 1
                        name: "check"
                        size: 15
                        strokeWidth: 2.4
                        color: "#5fd38d"
                    }

                    UI.Label {
                        Layout.fillWidth: true
                        text: modelData
                        size: 13
                        lineHeight: 1.4
                        wrapMode: Text.Wrap
                        elide: Text.ElideNone
                    }
                }
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        visible: dialog.canHold
        implicitHeight: holdRow.implicitHeight + 24
        radius: 10
        color: UI.Style.dark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(0, 0, 0, 0.03)
        border.width: 1
        border.color: UI.Style.dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.10)

        RowLayout {
            id: holdRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 12

            UI.CheckBox {
                Layout.alignment: Qt.AlignTop
                checked: dialog.hold
                Accessible.name: "Hold back the build " + (dialog.point.build || "") + " update for 2 weeks"
                onToggled: dialog.hold = checked
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                UI.Label {
                    Layout.fillWidth: true
                    text: "Hold back the build " + (dialog.point.build || "") + " update for 2 weeks"
                    size: 13
                    wrapMode: Text.Wrap
                }

                UI.Label {
                    Layout.fillWidth: true
                    text: "So it doesn't install again straight away. Security fixes still come through."
                    size: 12
                    color: UI.Style.inkMuted
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        PathIcon {
            name: "undo-2"
            size: 14
            strokeWidth: 2
            color: UI.Style.inkSubtle
        }

        UI.Label {
            Layout.fillWidth: true
            text: "Changed your mind later? Today's version is saved as a restore point too."
            size: 12.5
            color: UI.Style.inkMuted
            wrapMode: Text.Wrap
        }
    }

    UI.Label {
        Layout.fillWidth: true
        visible: Recovery.error.length > 0 && !Recovery.busy
        text: Recovery.error
        size: 12.5
        color: UI.Style.danger
        wrapMode: Text.Wrap
    }

    buttons: [
        UI.Button {
            implicitHeight: 32
            text: "Cancel"
            enabled: !Recovery.busy
            onClicked: dialog.close()
        },
        UI.Button {
            implicitHeight: 32
            kind: "primary"
            iconName: "restore"
            text: "Go back and restart"
            busy: Recovery.busy
            onClicked: Recovery.goBack(dialog.point.id, dialog.canHold && dialog.hold)
        }
    ]

    Connections {
        target: Recovery
        function onFinished(what) {
            if (what === "go-back" && dialog.opened) {
                dialog.close();
                Recovery.restart();
            }
        }
    }
}
