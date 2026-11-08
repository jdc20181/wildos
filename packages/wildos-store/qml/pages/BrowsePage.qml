import QtQuick
import QtQuick.Layouts
import WildOS.Store
import WildOS.UI as UI

// Apps and Games: every app in the catalogue, narrowed by category.
AppGrid {
    id: page

    property var store
    property string arg
    property string kind
    readonly property bool games: kind === "games"
    readonly property string categoryId: arg.length > 0 ? arg : (games ? "games" : "all")

    keys: Catalog.revision >= 0 ? Catalog.category(categoryId) : []
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
                title: page.games ? "Games" : "Apps"
                subtitle: page.games ? "Free games, and Steam for the ones you bought on Windows."
                                     : "Free apps for everything you did on Windows."
            }

            Flow {
                width: parent.width
                spacing: 8

                FilterPill {
                    text: page.games ? "All games" : "All apps"
                    selected: page.arg.length === 0
                    onClicked: page.store.open(page.kind, "")
                }

                Repeater {
                    model: page.games ? Catalog.gameCategories : Catalog.appCategories

                    FilterPill {
                        required property var modelData
                        text: modelData.label
                        selected: page.arg === modelData.id
                        onClicked: page.store.open(page.kind, modelData.id)
                    }
                }
            }

            UI.Label {
                visible: !Catalog.ready
                text: "Getting the list of apps"
                color: UI.Style.inkMuted
            }
        }
    }
}
