import QtQuick
import QtCore
import QtQuick.Dialogs
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// Background: every installed wallpaper, or a photo of your own. Changes
// apply straight away through plasma-apply-wallpaperimage.
PageScroll {
    id: page

    RowLayout {
        Layout.fillWidth: true
        spacing: 20

        Preview {
            Layout.preferredWidth: 320
            Layout.preferredHeight: 190
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 8

            UI.Label {
                Layout.fillWidth: true
                text: "Your background"
                size: 15
                font.weight: Font.DemiBold
            }

            UI.Label {
                Layout.fillWidth: true
                text: "Pictures look best when they are darker at the bottom, so the taskbar sits comfortably."
                color: UI.Style.inkMuted
                wrapMode: Text.Wrap
                elide: Text.ElideNone
            }

            UI.Button {
                Layout.topMargin: 6
                text: "Choose a photo"
                iconName: "image"
                onClicked: photoDialog.open()
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        SectionTitle {
            text: "Pictures"
        }

        WallpaperGrid {
            current: System.wallpaper
            onPicked: path => System.setWallpaper(path)
        }
    }

    FileDialog {
        id: photoDialog
        title: "Choose a photo"
        nameFilters: ["Pictures (*.jpg *.jpeg *.png *.webp)"]
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        onAccepted: System.setWallpaper(selectedFile.toString())
    }
}
