import QtQuick
import QtQuick.Templates as T
import WildOS.Store
import WildOS.UI as UI

// The compact app card from the bottom row of Store.dc.html: 34px icon, the
// name and one line under it. Used for every list of apps. A right click
// gives the app's menu, as tiles do in the Microsoft Store.
T.AbstractButton {
    id: tile

    property string appKey
    property string subtitle: app ? (app.busy || app.queued ? progressLabel : app.installed ? "Installed" : app.tagline) : ""
    readonly property var app: Catalog.app(appKey)
    readonly property string progressLabel: {
        if (!app) {
            return "";
        }
        if (app.queued) {
            return "Waiting to install";
        }
        const pct = app.progress >= 0 ? " " + app.progress + "%" : "";
        return (app.action === "remove" ? "Removing" : app.action === "update" ? "Updating" : "Installing") + pct;
    }

    signal activated(string key)

    implicitHeight: Math.max(62, Math.ceil(texts.implicitHeight) + 24)
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.Button
    Accessible.name: app ? app.name + ", " + subtitle : ""
    onClicked: activated(appKey)

    AppMenuArea {
        appKey: tile.appKey
    }

    background: Rectangle {
        radius: UI.Style.radiusMd
        color: tile.down ? UI.Style.surfacePressed : tile.hovered ? Metrics.tileHover : UI.Style.surfaceRaised
        border.width: 1
        border.color: UI.Style.borderSubtle
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: tile.visualFocus
            targetRadius: UI.Style.radiusMd
        }
    }

    contentItem: Item {
        AppIcon {
            id: icon
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            size: 34
            source: tile.app ? tile.app.iconSource : ""
        }

        Column {
            id: texts
            anchors.left: icon.right
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            UI.Label {
                width: parent.width
                text: tile.app ? tile.app.name : ""
                font.weight: Font.DemiBold
            }

            UI.Label {
                width: parent.width
                text: tile.subtitle
                size: 12
                color: tile.app && (tile.app.busy || tile.app.queued) ? UI.Style.accentText : UI.Style.inkMuted
            }
        }
    }
}
