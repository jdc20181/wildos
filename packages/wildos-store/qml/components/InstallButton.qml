import QtQuick
import WildOS.UI as UI

// The one action a card or row offers for an app: Install, Update or Open,
// turning into progress while something is happening to it.
Item {
    id: root

    property var app
    property string size: "small"

    signal opened()

    readonly property bool working: app && (app.busy || app.queued)

    implicitWidth: working ? pill.implicitWidth : button.implicitWidth
    implicitHeight: Math.max(button.implicitHeight, working ? pill.implicitHeight : 0)

    ProgressPill {
        id: pill
        anchors.fill: parent
        visible: root.working
        app: root.app
        fontSize: button.fontSize
    }

    UI.Button {
        id: button
        anchors.fill: parent
        visible: !root.working
        size: root.size
        kind: !root.app || !root.app.installed || root.app.updateAvailable ? "primary" : "secondary"
        // An app with no catalogue left to install it from cannot install.
        enabled: root.app && (root.app.installed ? root.app.updateAvailable || root.app.canOpen : root.app.source.length > 0)
        text: !root.app ? "Install"
            : !root.app.installed ? "Install"
            : root.app.updateAvailable ? "Update"
            : root.app.canOpen ? "Open" : "Installed"
        Accessible.name: root.app ? text + " " + root.app.name : text
        onClicked: {
            if (!root.app.installed) {
                root.app.install();
            } else if (root.app.updateAvailable) {
                root.app.update();
            } else if (root.app.open()) {
                root.opened();
            }
        }
    }
}
