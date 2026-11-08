import QtQuick
import QtQuick.Layouts
import WildOS.Store
import WildOS.UI as UI

// The full list behind "Swap your Windows apps": each Windows app people
// know, and the free app that does its job here.
PageFlick {
    id: page

    property var store
    property string arg
    property string kind

    readonly property var essentials: Catalog.revision >= 0 ? Catalog.essentials() : []
    readonly property int essentialsLeft: {
        let n = 0;
        for (const k of essentials) {
            const a = Catalog.app(k);
            if (a && !a.installed) {
                n++;
            }
        }
        return n;
    }
    readonly property bool essentialsBusy: {
        for (const k of essentials) {
            const a = Catalog.app(k);
            if (a && (a.busy || a.queued)) {
                return true;
            }
        }
        return false;
    }

    PageTitle {
        title: "Windows apps"
        subtitle: "Free apps that do what your Windows apps did. Most open the same files, so nothing gets left behind."

        UI.Button {
            size: "large"
            kind: "primary"
            enabled: page.essentialsLeft > 0 && !page.essentialsBusy
            text: page.essentialsBusy ? "Installing the essentials" : page.essentialsLeft === 0 && page.essentials.length > 0 ? "Essentials installed" : "Install the essentials"
            onClicked: Catalog.installEssentials()
        }
    }

    SwapGrid {
        keys: Catalog.revision >= 0 ? Catalog.swaps() : []
        onActivated: key => page.store.openApp(key)
    }

    RowGroup {
        Layout.fillWidth: true

        InfoRow {
            title: "Need the Windows app itself?"
            subtitle: "Wild OS can run many Windows programs in a sandbox. Open the .exe file from Files and Wild OS will offer to install it."
            last: true

            UI.Button {
                size: "small"
                anchors.verticalCenter: parent.verticalCenter
                text: "Windows app settings"
                onClicked: Catalog.openSettings("windowsapps")
            }
        }
    }
}
