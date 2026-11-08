import QtQuick
import QtQuick.Layouts
import WildOS.Store
import WildOS.UI as UI

// Search results. Typing a Windows app's name ("Photoshop") puts the apps
// that replace it first, as their own cards.
AppGrid {
    id: page

    property var store
    property string arg
    property string kind
    readonly property var swapKeys: Catalog.revision >= 0 ? Catalog.swapMatches(arg) : []

    keys: {
        const all = Catalog.revision >= 0 ? Catalog.search(arg) : [];
        return all.filter(k => swapKeys.indexOf(k) < 0);
    }
    onActivated: key => store.openApp(key)

    header: Item {
        width: page.width - page.leftMargin - page.rightMargin - 12
        height: headerColumn.implicitHeight + 22

        Column {
            id: headerColumn
            width: parent.width
            spacing: 16

            PageTitle {
                width: parent.width
                title: "Results for “" + page.arg + "”"
                subtitle: !Catalog.ready ? "Getting the list of apps"
                        : page.keys.length + page.swapKeys.length === 0 ? "Nothing matches. Try the name of the Windows app you used, or what you want to do, like “edit photos”." : ""
            }

            UI.Label {
                visible: page.swapKeys.length > 0
                text: "Instead of what you used on Windows"
                font.pixelSize: UI.Style.px(17)
                font.weight: Font.DemiBold
            }

            SwapGrid {
                visible: page.swapKeys.length > 0
                width: parent.width
                keys: page.swapKeys
                onActivated: key => page.store.openApp(key)
            }

            UI.Label {
                visible: page.swapKeys.length > 0 && page.keys.length > 0
                topPadding: 10
                text: "More apps"
                font.pixelSize: UI.Style.px(17)
                font.weight: Font.DemiBold
            }
        }
    }
}
