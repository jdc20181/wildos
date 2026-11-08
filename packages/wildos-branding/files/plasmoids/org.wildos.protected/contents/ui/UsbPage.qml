pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// The blocked USB devices, each as the design's decision card: what it is,
// then "Allow once" and "Keep blocked", the safe choice filled. Allowing
// lasts until the device is unplugged.
FocusScope {
    id: page

    required property var security

    signal back()

    implicitHeight: column.implicitHeight

    function focusFirst(): void {
        backButton.forceActiveFocus(Qt.TabFocusReason);
    }

    // What the device says it is, in words. A keyboard is called out because
    // a "memory stick" that types is the classic USB attack.
    function describe(kind: string): string {
        switch (kind) {
        case "keyboard": return "Says it's a keyboard";
        case "mouse": return "Mouse";
        case "input": return "Input device";
        case "storage": return "USB drive";
        case "network": return "Network adapter";
        case "wireless": return "Wireless adapter";
        case "camera": return "Camera";
        case "phone": return "Phone or camera";
        case "audio": return "Audio device";
        case "printer": return "Printer";
        case "hub": return "USB hub";
        }
        return "USB device";
    }

    ColumnLayout {
        id: column
        width: parent.width
        spacing: 12

        Item {
            Layout.fillWidth: true
            implicitHeight: 44

            UI.IconButton {
                id: backButton
                // The chevron lines up with the content edge, as on quick
                // settings' detail pages.
                x: -9
                anchors.verticalCenter: parent.verticalCenter
                width: 32
                height: 32
                iconName: "chevron-left"
                iconSize: 14
                stroke: 2.2
                tip: "Back to security"
                onClicked: page.back()
            }

            UI.Label {
                anchors.left: backButton.right
                anchors.leftMargin: 3
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: "Blocked USB devices"
                size: 14
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
            }
        }

        UI.Label {
            Layout.fillWidth: true
            Layout.topMargin: -6
            text: "Wild OS holds back new USB devices until you say they're OK. Only allow devices you recognise."
            color: UI.Style.inkMuted
            size: 12
            lineHeight: 1.3
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }

        Repeater {
            model: page.security.pendingDevices

            Rectangle {
                id: card

                required property var modelData
                readonly property bool busy: page.security.busy[modelData.port] === true
                readonly property string error: page.security.errors[modelData.port] || ""

                Layout.fillWidth: true
                implicitHeight: cardColumn.implicitHeight + 28
                radius: UI.Style.radiusLg
                color: UI.Style.surfaceControl

                ColumnLayout {
                    id: cardColumn
                    x: 14
                    y: 14
                    width: parent.width - 28
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        UI.Icon {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 1
                            name: "plug"
                            size: 20
                            strokeWidth: 2
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            UI.Label {
                                Layout.fillWidth: true
                                text: card.modelData.name.length > 0 ? card.modelData.name : "Unnamed USB device"
                                font.weight: Font.DemiBold
                            }

                            UI.Label {
                                Layout.fillWidth: true
                                text: card.busy ? "Waiting for permission" : page.describe(card.modelData.kind)
                                size: 12
                                color: card.modelData.kind === "keyboard" && !card.busy ? UI.Style.warning : UI.Style.inkStatus
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 32
                        columns: 2
                        columnSpacing: 8

                        UI.Button {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            size: "small"
                            text: "Allow once"
                            enabled: !card.busy
                            Accessible.name: "Allow " + (card.modelData.name || "this device") + " once"
                            onClicked: page.security.allowDevice(card.modelData.port)
                        }

                        UI.Button {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            size: "small"
                            text: "Keep blocked"
                            kind: "primary"
                            enabled: !card.busy
                            Accessible.name: "Keep " + (card.modelData.name || "this device") + " blocked"
                            onClicked: page.security.keepBlocked(card.modelData.port)
                        }
                    }

                    UI.Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 32
                        visible: card.error.length > 0
                        text: card.error
                        color: UI.Style.warning
                        size: 12
                        wrapMode: Text.Wrap
                        elide: Text.ElideNone
                    }
                }
            }
        }
    }
}
