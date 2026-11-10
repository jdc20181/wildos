import QtQuick
import QtQuick.Layouts
import WildOS.Store
import WildOS.UI as UI

// Library: every app installed from either catalogue, to open, update or
// uninstall from its menu.
AppGrid {
    id: page

    property var store
    property string arg
    property string kind

    keys: Catalog.catalogRevision + Catalog.stateRevision >= 0 ? Catalog.installedApps() : []
    showMore: true
    onActivated: key => store.openApp(key)

    header: Column {
        width: page.innerWidth
        spacing: 18
        bottomPadding: 4

        PageTitle {
            width: parent.width
            title: "Library"
            subtitle: !Catalog.ready ? "Getting the list of apps"
                : page.keys.length === 0 ? "No apps to show yet."
                : page.keys.length === 1 ? "1 app" : page.keys.length + " apps"
        }

        // What the sidebar's ring stands for: apps on their way in or out.
        SectionHeader {
            visible: working.keys.length > 0
            width: parent.width
            title: "Getting now"
        }

        AppList {
            id: working
            visible: keys.length > 0
            width: parent.width
            keys: Catalog.stateRevision >= 0 ? Catalog.workingApps() : []
            onActivated: key => page.store.openApp(key)
        }

        Rectangle {
            visible: page.keys.length > 0
            width: parent.width
            height: 1
            color: Metrics.divider
        }
    }
}
