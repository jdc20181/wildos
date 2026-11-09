import QtQuick
import QtCore
import QtQuick.Dialogs
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Personalisation, laid out as Personalisation.dc.html: a preview of the
// desktop beside the theme and accent, then the desktop settings. Everything
// applies at once through the same tools Plasma itself uses.
PageScroll {
    id: page

    RowLayout {
        Layout.fillWidth: true
        spacing: 20

        DesktopPreview {
            Layout.preferredWidth: 320
            Layout.preferredHeight: 190
            Layout.alignment: Qt.AlignTop
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                UI.Label {
                    text: "Theme"
                    size: 14
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                }

                // Light and Automatic need the light colour scheme installed;
                // without it they are held back with the reason on hover.
                // Automatic switches at sunrise and sunset (wildos-auto-theme).
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: [
                            // Each theme's page ground, as UI.Style draws it.
                            { name: "Dark", scheme: "WildOS", swatch: ["#0f1326"] },
                            { name: "Light", scheme: "WildOSLight", swatch: ["#eef1f7"] },
                            { name: "Automatic", scheme: "", swatch: ["#eef1f7", "#0f1326"] }
                        ]

                        Item {
                            id: theme
                            required property var modelData
                            readonly property bool automatic: modelData.scheme.length === 0
                            readonly property bool available: System.hasColourScheme(automatic ? "WildOSLight" : modelData.scheme)
                            readonly property string reason: "The light theme is not installed on this computer"
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            implicitHeight: card.implicitHeight

                            HoverHandler {
                                id: themeHover
                            }

                            UI.Tooltip {
                                text: theme.reason
                                visible: !theme.available && themeHover.hovered
                            }

                            UI.ChoiceCard {
                                id: card
                                width: parent.width
                                padding: 13
                                radius: 10
                                checkable: false
                                checked: theme.automatic ? System.automaticTheme
                                    : !System.automaticTheme && (System.colourScheme === theme.modelData.scheme
                                        || (theme.modelData.scheme === "WildOS" && System.colourScheme.length === 0))
                                enabled: theme.available
                                Accessible.name: theme.modelData.name
                                Accessible.description: theme.available ? "" : theme.reason
                                onClicked: theme.automatic ? System.setAutomaticTheme() : System.setColourScheme(theme.modelData.scheme)

                                Row {
                                    spacing: 10

                                    Rectangle {
                                        width: 22
                                        height: 22
                                        radius: 6
                                        clip: true
                                        color: theme.modelData.swatch[0]
                                        border.width: 1
                                        border.color: UI.Style.borderControl

                                        Rectangle {
                                            visible: theme.modelData.swatch.length > 1
                                            x: 11
                                            width: 11
                                            height: 22
                                            color: theme.modelData.swatch[1] || "transparent"
                                        }
                                    }

                                    UI.Label {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: theme.modelData.name
                                        font.weight: card.checked ? Font.DemiBold : Font.Normal
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                UI.Label {
                    text: "Accent colour"
                    size: 14
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                }

                AccentPicker {
                    Layout.leftMargin: 5
                    Layout.topMargin: 5
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Desktop"
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    title: "Wallpaper"
                    description: System.wallpaperName
                    interactive: true
                    onClicked: Navigator.open("background")

                    UI.Button {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Browse photos"
                        onClicked: photoDialog.open()
                    }
                }

                UI.SettingRow {
                    title: "Change wallpaper automatically"
                    description: System.slideshow && System.slideshowFolder.length > 0
                        ? "A new photo from " + System.slideshowFolder.split("/").pop() + " every half hour"
                        : "Cycle through a folder of your photos"
                    status: (System.slideshow) ? "On" : "Off"

                    UI.Toggle {
                        id: slideshowToggle
                        anchors.verticalCenter: parent.verticalCenter
                        checked: System.slideshow
                        Accessible.name: "Change wallpaper automatically"
                        onToggled: {
                            if (checked) {
                                folderDialog.open();
                            } else {
                                System.setSlideshow(false);
                            }
                        }
                    }
                }

                UI.SettingRow {
                    title: "Taskbar layout"
                    description: "Centred like Windows 11, or left-aligned like Windows 10"

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "Taskbar layout"
                        model: [
                            { value: true, text: "Centred" },
                            { value: false, text: "Left" }
                        ]
                        readonly property var choice: System.taskbarCentred
                        currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                        onActivated: index => System.setTaskbarCentred(model[index].value)
                    }
                }

                UI.SettingRow {
                    title: "Taskbar style"
                    description: "Docked along the bottom like Windows, or floating panels"

                    UI.ComboBox {
                        anchors.verticalCenter: parent.verticalCenter
                        Accessible.name: "Taskbar style"
                        model: [
                            { value: false, text: "Docked" },
                            { value: true, text: "Floating" }
                        ]
                        readonly property var choice: System.taskbarFloating
                        currentIndex: model ? model.findIndex(o => o.value === choice) : -1
                        onActivated: index => System.setTaskbarFloating(model[index].value)
                    }
                }

                UI.SettingRow {
                    title: "Transparency effects"
                    description: "The frosted glass behind menus and windows"
                    status: (System.transparency) ? "On" : "Off"

                    UI.Toggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: System.transparency
                        Accessible.name: "Transparency effects"
                        onToggled: System.setTransparency(checked)
                    }
                }

                UI.SettingRow {
                    title: "Text size"
                    description: "Makes text larger across every app"

                    TextSizeBox {
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            Repeater {
                model: ["background", "lockscreen", "taskbar", "fonts"]

                UI.SettingRow {
                    required property string modelData
                    readonly property var info: Pages.page(modelData)
                    iconName: info.icon
                    title: info.title
                    description: info.description || ""
                    chevron: true
                    onClicked: Navigator.open(modelData)
                }
            }
        }
    }

    FileDialog {
        id: photoDialog
        title: "Choose a photo"
        nameFilters: ["Pictures (*.jpg *.jpeg *.png *.webp)"]
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        onAccepted: System.setWallpaper(selectedFile.toString())
    }

    FolderDialog {
        id: folderDialog
        title: "Choose a folder of photos"
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        onAccepted: System.setSlideshow(true, selectedFolder.toString())
        // No folder, no slideshow: the switch goes back to off.
        onRejected: slideshowToggle.checked = Qt.binding(() => System.slideshow)
    }
}
