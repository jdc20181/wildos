import QtQuick
import WildOS.Music
import WildOS.UI as UI

// Every artist as a round tile, A to Z ("The" aside).
TileGrid {
    id: page

    property var app

    round: true
    minimumTile: 150
    model: {
        Library.revision;
        return Library.artists();
    }

    onOpened: item => app.navigate("artist", item.name)
    onPlayed: item => app.play(Library.artist(item.name).tracks.map(t => t.path), 0, false)
    onMenuAt: (item, tile, x, y) => app.collectionMenu("artist", item.name, Library.artist(item.name).tracks.map(t => t.path), tile, x, y)

    heading: Component {
        PageHeading {
            title: qsTr("Artists")
            subtitle: Library.artistCount === 1 ? qsTr("1 artist") : qsTr("%1 artists").arg(MusicStyle.count(Library.artistCount))
        }
    }
}
