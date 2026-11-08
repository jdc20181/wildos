import QtQuick
import QtQuick.Layouts
import WildOS.Store
import WildOS.UI as UI

// App updates from both catalogues. Updates to Wild OS itself are in
// Settings, as Windows keeps the Store and Windows Update apart.
PageFlick {
    id: page

    property var store
    property string arg
    property string kind
    readonly property var keys: Catalog.revision >= 0 ? Catalog.updates() : []

    PageTitle {
        title: "Updates"
        subtitle: page.keys.length === 1 ? "1 app has an update."
                : page.keys.length > 1 ? page.keys.length + " apps have updates."
                : Catalog.checking ? "Checking for updates."
                : Catalog.checkResult === "failed" ? "Couldn't check every app for updates. Check you're online and try again."
                : Catalog.checkResult === "ok" ? "Your apps are up to date. Checked at " + Catalog.checkedAt + "."
                : "Your apps are up to date."

        UI.Button {
            size: "large"
            visible: page.keys.length === 0
            enabled: !Catalog.checking
            text: Catalog.checking ? "Checking" : "Check for updates"
            onClicked: Catalog.refresh()
        }

        UI.Button {
            size: "large"
            kind: "primary"
            visible: page.keys.length > 0
            enabled: !Catalog.busy
            text: "Update all"
            onClicked: Catalog.updateAll()
        }
    }

    RowGroup {
        visible: page.keys.length > 0
        Layout.fillWidth: true

        Repeater {
            model: page.keys

            AppRow {
                required property string modelData
                appKey: modelData
                onActivated: key => page.store.openApp(key)
            }
        }
    }

    RowGroup {
        Layout.fillWidth: true

        InfoRow {
            title: "Wild OS updates"
            subtitle: "Security fixes and updates for Wild OS itself are in Settings."
            last: true

            UI.Button {
                size: "small"
                anchors.verticalCenter: parent.verticalCenter
                text: "Open Settings"
                onClicked: Catalog.openSettings("update")
            }
        }
    }
}
