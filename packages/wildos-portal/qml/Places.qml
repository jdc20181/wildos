pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import WildOS.UI as UI

// The places column of DialogSave.dc: 190 wide, 32px rows with a 16px line
// icon and the name in 13px, the folder you are in lit with weight 600.
// The same places as the Files sidebar, in the same order.
Item {
    id: places

    required property var folder

    readonly property var entries: {
        const home = StandardPaths.writableLocation(StandardPaths.HomeLocation);
        const list = [{ name: "Home", icon: "home", url: home }];
        const add = (name, icon, location) => {
            const url = StandardPaths.writableLocation(location);
            if (url.toString().length > 0 && url.toString() !== home.toString()) {
                list.push({ name: name, icon: icon, url: url });
            }
        };
        add("Desktop", "desktop", StandardPaths.DesktopLocation);
        add("Documents", "document", StandardPaths.DocumentsLocation);
        add("Downloads", "download", StandardPaths.DownloadLocation);
        add("Pictures", "pictures", StandardPaths.PicturesLocation);
        add("Music", "music", StandardPaths.MusicLocation);
        add("Videos", "videos", StandardPaths.MoviesLocation);
        list.push({ name: "This computer", icon: "computer", url: "wildos-computer:/" });
        return list;
    }

    function same(a, b) {
        const strip = s => s.toString().replace(/\/+$/, "");
        return strip(a) === strip(b);
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: 10
        contentHeight: column.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: UI.ScrollBar {}

        Column {
            id: column
            width: flick.width
            spacing: 2

            Repeater {
                model: places.entries

                delegate: Rectangle {
                    id: place

                    required property var modelData
                    readonly property bool current: places.same(places.folder.url, modelData.url)

                    width: column.width
                    height: 32
                    radius: UI.Style.radiusXs
                    color: placeMouse.pressed ? UI.Style.surfacePressed : current || placeMouse.containsMouse ? UI.Style.surfaceHover : "transparent"
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.name
                    Accessible.onPressAction: places.folder.openUrl(modelData.url)
                    Keys.onReturnPressed: places.folder.openUrl(modelData.url)
                    Keys.onSpacePressed: places.folder.openUrl(modelData.url)

                    UI.FocusRing {
                        visible: place.activeFocus
                        targetRadius: place.radius
                    }

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 10
                        spacing: 10

                        UI.Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: place.modelData.icon
                            size: 16
                        }

                        UI.Label {
                            anchors.verticalCenter: parent.verticalCenter
                            width: place.width - 46
                            text: place.modelData.name
                            font.weight: place.current ? UI.Style.weightSemiBold : UI.Style.weightRegular
                        }
                    }

                    MouseArea {
                        id: placeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: places.folder.openUrl(place.modelData.url)
                    }
                }
            }
        }
    }
}
