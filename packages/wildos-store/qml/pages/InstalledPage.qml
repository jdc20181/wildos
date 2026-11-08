import QtQuick
import QtQuick.Layouts
import WildOS.Store

// Every app installed from either catalogue, to open or remove.
PageFlick {
    id: page

    property var store
    property string arg
    property string kind
    readonly property var keys: Catalog.revision >= 0 ? Catalog.installedApps() : []

    PageTitle {
        title: "Installed"
        subtitle: !Catalog.ready ? "Getting the list of apps"
                : page.keys.length === 1 ? "1 app" : page.keys.length + " apps"
    }

    RowGroup {
        visible: page.keys.length > 0
        Layout.fillWidth: true

        Repeater {
            model: page.keys

            AppRow {
                required property string modelData
                appKey: modelData
                showRemove: true
                onActivated: key => page.store.openApp(key)
            }
        }
    }
}
