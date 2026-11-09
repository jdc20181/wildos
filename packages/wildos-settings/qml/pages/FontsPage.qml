import QtQuick
import QtCore
import QtQuick.Dialogs
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Fonts, after Windows 11's Personalisation > Fonts: install a font file
// for your account, search the fonts on this computer with a line of each,
// and remove the ones you installed. Text size is on Personalisation.
PageScroll {
    id: page

    readonly property int pageSize: 60
    property int shown: pageSize
    readonly property string query: search.text.trim().toLowerCase()
    readonly property var matches: query.length === 0 ? Fonts.families
        : Fonts.families.filter(f => f.family.toLowerCase().includes(query))
    readonly property string sample: "The quick brown fox jumps over the lazy dog"

    onQueryChanged: shown = pageSize

    Component.onCompleted: Fonts.reload()

    Connections {
        target: Fonts
        function onFinished(title, message, failed) {
            notice.show(title, message, failed);
        }
    }

    Notice {
        id: notice
    }

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            UI.SettingRow {
                iconName: "font"
                title: "Add fonts"
                description: "Install a font file (.ttf or .otf) for your account"

                UI.Spinner {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: Fonts.busy
                }

                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Browse and install fonts"
                    enabled: !Fonts.busy
                    onClicked: fontDialog.open()
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            SectionTitle {
                Layout.fillWidth: true
                text: "Available fonts"
            }

            UI.SearchField {
                id: search
                Layout.preferredWidth: 260
                placeholderText: "Search fonts"
            }
        }

        UI.Label {
            Layout.fillWidth: true
            text: page.matches.length === 1 ? "1 font" : page.matches.length + " fonts"
            size: 12
            color: UI.Style.inkMuted
        }

        UI.Card {
            Layout.fillWidth: true

            Column {
                width: parent.width

                UI.SettingRow {
                    visible: page.matches.length === 0
                    title: Fonts.families.length === 0 ? "Looking for fonts" : "No fonts match"
                }

                Repeater {
                    model: page.matches.slice(0, page.shown)

                    UI.SettingRow {
                        id: fontRow
                        required property var modelData
                        title: modelData.family
                        description: (modelData.styles === 1 ? "1 style" : modelData.styles + " styles")
                            + (modelData.removable ? ", installed by you" : "")

                        below: UI.Label {
                            width: fontRow.width - 120
                            text: page.sample
                            size: 18
                            font.family: fontRow.modelData.family
                            elide: Text.ElideRight
                        }

                        UI.IconButton {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: fontRow.modelData.removable
                            enabled: !Fonts.busy
                            iconName: "delete"
                            tip: "Remove " + fontRow.modelData.family
                            onClicked: {
                                confirm.family = fontRow.modelData.family;
                                confirm.open();
                            }
                        }
                    }
                }
            }
        }

        UI.Button {
            visible: page.matches.length > page.shown
            text: "Show more fonts"
            onClicked: page.shown += page.pageSize
        }
    }

    FileDialog {
        id: fontDialog
        title: "Choose fonts to install"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Fonts (*.ttf *.otf *.ttc *.otc *.TTF *.OTF)"]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DownloadLocation)
        onAccepted: Fonts.install(selectedFiles)
    }

    UI.Dialog {
        id: confirm

        property string family

        title: "Remove " + family + "?"
        text: "Apps you open from now on will not have " + family + ". Documents that use it will show another font."
        buttons: [
            UI.Button {
                text: "Remove"
                kind: "primary"
                onClicked: {
                    confirm.close();
                    Fonts.remove(confirm.family);
                }
            },
            UI.Button {
                text: "Keep it"
                onClicked: confirm.close()
            }
        ]
    }
}
