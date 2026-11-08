import QtQuick
import QtQuick.Templates as T
import WildOS.Store
import WildOS.UI as UI

// "Instead of Photoshop": the Windows app on top, the free one that replaces
// it, and why it is a good swap, as in Store.dc.html.
T.AbstractButton {
    id: card

    property string appKey
    readonly property var app: Catalog.app(appKey)

    signal activated(string key)

    implicitHeight: column.implicitHeight + 32
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.Button
    Accessible.name: app ? "Instead of " + app.insteadOf + ", " + app.name : ""
    onClicked: activated(appKey)

    AppMenuArea {
        appKey: card.appKey
    }

    background: Rectangle {
        radius: UI.Style.radiusLg
        color: card.down ? UI.Style.surfacePressed : card.hovered ? Metrics.tileHover : UI.Style.surfaceRaised
        border.width: 1
        border.color: UI.Style.borderSubtle
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: card.visualFocus
            targetRadius: UI.Style.radiusLg
        }
    }

    contentItem: Item {
        Column {
            id: column
            x: 16
            y: 16
            width: parent.width - 32
            spacing: 14

            Row {
                spacing: 8

                UI.Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: card.app ? "Instead of " + card.app.insteadOf : ""
                    size: 12
                    color: Metrics.inkCard
                    width: Math.min(implicitWidth, column.width - 20)
                }

                UI.Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "arrow"
                    size: 12
                    strokeWidth: 2.2
                    color: Metrics.inkCard
                }
            }

            Row {
                width: parent.width
                spacing: 14

                AppIcon {
                    id: icon
                    size: 48
                    source: card.app ? card.app.iconSource : ""
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - icon.width - 14
                    spacing: 3

                    UI.Label {
                        width: parent.width
                        text: card.app ? card.app.name : ""
                        size: 15
                        font.weight: Font.DemiBold
                    }

                    UI.Label {
                        width: parent.width
                        text: card.app ? card.app.tagline : ""
                        size: 12
                        color: Metrics.inkCard
                    }
                }
            }

            Item {
                width: parent.width
                height: action.height

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    width: parent.width - action.width - 12

                    UI.Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "shield"
                        size: 13
                        strokeWidth: 2.4
                        color: UI.Style.accent
                    }

                    UI.Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 19
                        text: card.app ? card.app.cardNote : ""
                        size: 12
                        color: UI.Style.accentText
                    }
                }

                InstallButton {
                    id: action
                    anchors.right: parent.right
                    width: implicitWidth
                    app: card.app
                }
            }
        }
    }
}
