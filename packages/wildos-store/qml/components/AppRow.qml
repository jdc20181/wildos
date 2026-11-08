import QtQuick
import QtQuick.Templates as T
import WildOS.Store
import WildOS.UI as UI

// An app as a row in Updates and Installed: icon, name, where it came from,
// and its actions on the right.
T.AbstractButton {
    id: row

    property string appKey
    property bool showRemove: false
    readonly property var app: Catalog.app(appKey)

    signal activated(string key)

    width: parent ? parent.width : implicitWidth
    implicitHeight: Math.max(62, Math.ceil(texts.implicitHeight) + 24)
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.Button
    Accessible.name: app ? app.name : ""
    onClicked: activated(appKey)

    AppMenuArea {
        appKey: row.appKey
    }

    background: Rectangle {
        color: row.down ? UI.Style.surfacePressed : row.hovered ? UI.Style.surfaceHover : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: UI.Style.divider
        }

        UI.FocusRing {
            visible: row.visualFocus
            anchors.margins: 2
            targetRadius: UI.Style.radiusXs
        }
    }

    contentItem: Item {
        AppIcon {
            id: icon
            x: 18
            anchors.verticalCenter: parent.verticalCenter
            size: 34
            source: row.app ? row.app.iconSource : ""
        }

        Column {
            id: texts
            anchors.left: icon.right
            anchors.leftMargin: 14
            anchors.right: actions.left
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            UI.Label {
                width: parent.width
                text: row.app ? row.app.name : ""
                size: 14
            }

            UI.Label {
                width: parent.width
                size: 12
                color: UI.Style.inkMuted
                text: {
                    if (!row.app) {
                        return "";
                    }
                    const from = row.app.source === "flatpak" ? "Sandboxed app" : "Wild OS app";
                    const extra = row.app.sizeText.length > 0 ? row.app.sizeText : row.app.version.length > 0 ? "Version " + row.app.version : "";
                    return extra.length > 0 ? from + ", " + extra.charAt(0).toLowerCase() + extra.slice(1) : from;
                }
            }
        }

        Row {
            id: actions
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            UI.Button {
                visible: row.showRemove && row.app && row.app.installed && !row.app.busy && !row.app.queued
                size: "small"
                text: "Uninstall"
                onClicked: removeDialog.open()
            }

            InstallButton {
                app: row.app
            }
        }
    }

    UI.Dialog {
        id: removeDialog
        title: row.app ? "Uninstall " + row.app.name + "?" : ""
        text: "Your files stay where they are."
        buttons: [
            UI.Button {
                text: "Cancel"
                onClicked: removeDialog.close()
            },
            UI.Button {
                kind: "primary"
                text: "Uninstall"
                onClicked: {
                    removeDialog.close();
                    row.app.remove();
                }
            }
        ]
    }
}
