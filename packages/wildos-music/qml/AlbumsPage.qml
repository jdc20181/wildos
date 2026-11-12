import QtQuick
import WildOS.Music
import WildOS.UI as UI

// Every album as a grid of covers, in title, artist, year or newest order.
TileGrid {
    id: page

    property var app
    property string sort: "title"

    model: {
        Library.revision;
        return Library.albums(sort);
    }

    onOpened: item => app.navigate("album", item.key)
    onPlayed: item => app.play(Library.album(item.key).tracks.map(t => t.path), 0, false)
    onMenuAt: (item, tile, x, y) => app.collectionMenu("album", item.key, Library.album(item.key).tracks.map(t => t.path), tile, x, y)

    heading: Component {
        PageHeading {
            title: qsTr("Albums")
            subtitle: Library.albumCount === 1 ? qsTr("1 album") : qsTr("%1 albums").arg(MusicStyle.count(Library.albumCount))

            PageButton {
                id: sortButton
                iconName: "sort"
                text: page.sort === "artist" ? qsTr("Artist") : page.sort === "year" ? qsTr("Year") : page.sort === "added" ? qsTr("Recently added") : qsTr("Title")
                onClicked: sortMenu.popup(sortButton, 0, sortButton.height + 4)

                UI.Menu {
                    id: sortMenu
                    entries: [
                        { text: qsTr("Title"), radio: true, checked: page.sort === "title", run: () => page.sort = "title" },
                        { text: qsTr("Artist"), radio: true, checked: page.sort === "artist", run: () => page.sort = "artist" },
                        { text: qsTr("Year"), radio: true, checked: page.sort === "year", run: () => page.sort = "year" },
                        { text: qsTr("Recently added"), radio: true, checked: page.sort === "added", run: () => page.sort = "added" }
                    ]
                }
            }
        }
    }
}
