import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import WildOS.Settings
import WildOS.UI as UI

// Home, after Windows 11's: the PC and its name at the top with network and
// update status beside it, then cards for the things people change most.
PageScroll {
    id: page

    // Network and Bluetooth come from their own desktop modules; if either is
    // missing the card says so instead of the whole page failing to load.
    readonly property QtObject network: create("NetworkSummary")
    readonly property QtObject bluetooth: create("BluetoothSummary")
    readonly property var disk: System.volumes()[0]
    readonly property var backgrounds: System.wallpapers().slice(0, 4)
    property date now: new Date()

    function create(name) {
        const component = Qt.createComponent("WildOS.Settings", name);
        return component.status === Component.Ready ? component.createObject(page) : null;
    }

    Timer {
        interval: 30000
        running: true
        repeat: true
        onTriggered: page.now = new Date()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 18

        Rectangle {
            Layout.preferredWidth: 150
            Layout.preferredHeight: 94
            radius: UI.Style.radiusSm + 2
            color: UI.Style.surfaceSunken
            border.width: 1
            border.color: UI.Style.borderGlass

            Image {
                anchors.fill: parent
                anchors.margins: 4
                source: System.thumbnail(System.wallpaper)
                fillMode: Image.PreserveAspectCrop
                sourceSize.width: 300
                asynchronous: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            // The computer name is never cut short; the links beside it give
            // way instead.
            Layout.minimumWidth: hostLabel.implicitWidth
            spacing: 2

            UI.Label {
                id: hostLabel
                Layout.fillWidth: true
                text: System.hostName
                size: 20
                font.weight: Font.DemiBold
            }

            UI.Label {
                Layout.fillWidth: true
                text: System.deviceModel.length > 0 ? System.deviceModel : System.osName + " " + System.osVersion
                color: UI.Style.inkMuted
            }

            UI.LinkButton {
                Layout.leftMargin: -4
                text: "Rename"
                onClicked: Navigator.open("about")
            }
        }

        HomeLink {
            Layout.fillWidth: true
            Layout.maximumWidth: 230
            Layout.preferredWidth: 230
            iconName: page.network ? page.network.iconName : "globe"
            title: page.network ? page.network.title : "Network and internet"
            caption: page.network ? page.network.caption : "Not available"
            onClicked: Navigator.open("network")
        }

        HomeLink {
            Layout.fillWidth: true
            Layout.maximumWidth: 230
            Layout.preferredWidth: 230
            iconName: "update"
            iconColor: Updates.updates.length > 0 ? UI.Style.accent : UI.Style.iconInk
            title: Updates.updates.length > 0
                ? Format.plural(Updates.updates.length, "update", "updates") + " ready"
                : "Updates"
            caption: {
                page.now;
                const when = Format.ago(Updates.lastChecked);
                return when.length > 0 ? "Last checked " + when : "Not checked yet";
            }
            onClicked: Navigator.open("update")
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 10
        rowSpacing: 10

        HomeCard {
            iconName: "palette"
            title: "Personalise your device"
            caption: "Pick a background and an accent colour"
            linkText: "Browse more backgrounds"
            onLinkClicked: Navigator.open("background")

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: page.backgrounds

                    WallpaperThumb {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 58
                        source: modelData.thumbnail
                        selected: modelData.path === System.wallpaper
                        showTick: false
                        Accessible.name: "Use " + modelData.name + " as background"
                        onClicked: System.setWallpaper(modelData.path)
                    }
                }
            }

            AccentPicker {
                Layout.leftMargin: 5
                Layout.topMargin: 4
                size: 26
            }
        }

        HomeCard {
            iconName: "bluetooth"
            title: "Bluetooth devices"
            caption: page.bluetooth ? page.bluetooth.caption : "Bluetooth is not available"
            linkText: "View all devices"
            onLinkClicked: Navigator.open("bluetooth")

            RowLayout {
                Layout.fillWidth: true
                visible: page.bluetooth !== null && page.bluetooth.present
                spacing: 16

                UI.Label {
                    Layout.fillWidth: true
                    text: "Bluetooth"
                    size: 14
                }

                UI.Label {
                    text: page.bluetooth && page.bluetooth.on ? "On" : "Off"
                    size: 12
                    color: UI.Style.inkMuted
                }

                UI.Toggle {
                    checked: page.bluetooth !== null && page.bluetooth.on
                    Accessible.name: "Bluetooth"
                    onToggled: page.bluetooth.setOn(checked)
                }
            }
        }

        HomeCard {
            iconName: "storage"
            title: "Storage"
            caption: page.disk ? System.formatBytes(page.disk.free) + " free of " + System.formatBytes(page.disk.total) : ""
            linkText: "Manage storage"
            onLinkClicked: Navigator.open("storage")

            UI.ProgressBar {
                Layout.fillWidth: true
                barHeight: 8
                value: page.disk ? 1 - page.disk.free / page.disk.total : 0
                warning: value > 0.9
            }

            UI.Label {
                Layout.fillWidth: true
                text: page.disk ? System.formatBytes(page.disk.total - page.disk.free) + " used on the system disk" : ""
                size: 12
                color: UI.Style.inkMuted
            }
        }

        HomeCard {
            iconName: Protection.busy ? "update" : Protection.allOn ? "shield-check" : "shield-alert"
            title: Protection.busy ? "Checking your protection" : Protection.headline
            caption: {
                page.now;
                if (Protection.busy) {
                    return Protection.checkStep;
                }
                const when = Format.ago(System.securityChecked);
                return when.length > 0 ? "Checked " + when : "";
            }
            linkText: "Privacy and security"
            onLinkClicked: Navigator.open("privacy")

            // Each chip opens what that protection means and what to do
            // about it, the same dialog its tile opens on Privacy.
            Flow {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    // The four main protections, with anything that needs a
                    // look put first, so the chip the headline is about is
                    // always on the card.
                    model: Protection.attention.concat(Protection.items.filter(item => !Protection.attention.includes(item))).slice(0, Math.max(4, Protection.attention.length))

                    T.AbstractButton {
                        id: chip
                        required property var modelData
                        readonly property bool off: !Protection.good(modelData)
                        readonly property color tint: off ? UI.Style.warning : UI.Style.accent
                        implicitWidth: chipText.implicitWidth + 24
                        implicitHeight: Math.max(26, chipText.implicitHeight + 8)
                        padding: 0
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.title + ", " + Protection.value(modelData)
                        onClicked: protectionDialog.show(modelData)

                        background: Rectangle {
                            radius: height / 2
                            color: Qt.rgba(chip.tint.r, chip.tint.g, chip.tint.b, chip.down ? 0.26 : chip.hovered ? 0.20 : 0.12)
                            border.width: 1
                            border.color: Qt.rgba(chip.tint.r, chip.tint.g, chip.tint.b, 0.30)
                            Behavior on color {
                                ColorAnimation { duration: UI.Style.hoverDuration }
                            }

                            UI.FocusRing {
                                visible: chip.visualFocus
                                targetRadius: height / 2
                            }
                        }

                        contentItem: UI.Label {
                            id: chipText
                            horizontalAlignment: Text.AlignHCenter
                            text: chip.modelData.title
                            size: 12
                            color: chip.off ? UI.Style.warning : UI.Style.accentText
                        }
                    }
                }
            }

            UI.Button {
                text: Protection.busy ? "Checking" : "Check now"
                busy: Protection.busy
                onClicked: Protection.checkNow()
            }
        }
    }

    ProtectionDialog {
        id: protectionDialog
    }
}
