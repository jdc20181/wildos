import QtQuick
import QtQuick.Layouts
import WildOS.Settings
import WildOS.UI as UI

// A section such as System: one row per page inside it, as Windows 11 lists
// Display, Sound and the rest.
PageScroll {
    id: page

    readonly property string pageId: Navigator.window ? Navigator.window.currentId : ""

    UI.Card {
        Layout.fillWidth: true

        Column {
            width: parent.width

            Repeater {
                model: Pages.children(page.pageId)

                UI.SettingRow {
                    required property string modelData
                    readonly property var info: Pages.page(modelData)
                    iconName: info.icon
                    title: info.title
                    description: info.description || ""
                    chevron: !info.window
                    external: info.window !== undefined
                    onClicked: Navigator.open(modelData)
                }
            }
        }
    }
}
