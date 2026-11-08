pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import WildOS.Files
import WildOS.UI as UI

// The places column from FilesDetails.dc.html: the Home folders, then
// This computer and the drives, then the Recycle Bin.
Item {
    id: sidebar

    required property var folder
    required property var view
    readonly property int currentRow: folder ? PlacesModel.rowFor(folder.url) : -1

    width: Metrics.sidebarWidth

    Rectangle {
        anchors.fill: parent
        color: Metrics.sidebar
        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: UI.Style.divider
        }
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        anchors.leftMargin: 8
        anchors.rightMargin: 9
        spacing: 1
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: PlacesModel
        ScrollBar.vertical: UI.ScrollBar {}
        Accessible.role: Accessible.List
        Accessible.name: "Places"

        delegate: Item {
            id: place

            required property string name
            required property string iconName
            required property url url
            required property string kind
            required property string udi
            required property bool removable
            required property int index
            readonly property bool current: index === sidebar.currentRow

            width: ListView.view.width
            height: kind === "separator" ? 17 : Metrics.placeHeight

            Rectangle {
                visible: place.kind === "separator"
                x: 10
                width: parent.width - 20
                height: 1
                anchors.verticalCenter: parent.verticalCenter
                color: Metrics.rule
            }

            T.AbstractButton {
                id: button
                visible: place.kind !== "separator"
                anchors.fill: parent
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                Accessible.role: Accessible.Button
                Accessible.name: place.name
                onClicked: sidebar.open(place, false)

                background: Rectangle {
                    radius: UI.Style.radiusXs
                    color: button.down ? UI.Style.surfacePressed
                         : button.hovered || eject.hovered ? UI.Style.surfaceHover
                         : place.current ? UI.Style.surfaceSelected : "transparent"
                    Behavior on color {
                        ColorAnimation { duration: UI.Style.hoverDuration }
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.MiddleButton | Qt.RightButton
                    onTapped: (point, pressed) => {
                        if (pressed === Qt.MiddleButton) {
                            sidebar.open(place, true);
                        } else {
                            sidebar.view.openPlaceMenu(button, point.position.x, point.position.y, place);
                        }
                    }
                }

                UI.Icon {
                    id: icon
                    x: 10
                    anchors.verticalCenter: parent.verticalCenter
                    name: place.iconName
                    size: 16
                    strokeWidth: 1.8
                    color: place.current ? UI.Style.accent : UI.Style.ink
                }

                UI.Label {
                    anchors.left: icon.right
                    anchors.leftMargin: 10
                    anchors.right: eject.visible ? eject.left : parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: place.name
                    font.weight: place.current ? Font.Medium : Font.Normal
                }

                UI.IconButton {
                    id: eject
                    visible: place.removable && place.url.toString().length > 0 && (button.hovered || hovered)
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 24
                    height: 24
                    radius: UI.Style.radiusXs
                    iconName: "eject"
                    iconSize: 13
                    tip: "Eject " + place.name
                    focusPolicy: Qt.NoFocus
                    onClicked: FileOps.ejectDrive(place.udi)
                }

                DropArea {
                    anchors.fill: parent
                    enabled: place.kind !== "computer" && place.url.toString().length > 0
                    onEntered: drag => drag.accept(drag.proposedAction)
                    onDropped: drop => sidebar.view.dropUrls(drop, place.url)
                }
            }
        }
    }

    function open(place, inNewTab) {
        if (place.kind === "drive" && place.url.toString().length === 0) {
            if (inNewTab) {
                view.newTab("wildos:computer");
            }
            view.folder.openDrive(place.udi);
            return;
        }
        if (inNewTab) {
            view.newTab(place.url);
        } else {
            folder.openUrl(place.url);
            view.focusContent();
        }
    }
}
