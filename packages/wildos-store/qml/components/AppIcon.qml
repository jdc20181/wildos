import QtQuick

// An app's icon from the catalogue, the icon theme or the network, drawn at
// twice the size it shows so it stays sharp on high density screens.
Item {
    id: root

    property string source
    property real size: 48

    implicitWidth: size
    implicitHeight: size

    Image {
        id: image
        anchors.fill: parent
        source: root.source
        sourceSize.width: root.size * 2
        sourceSize.height: root.size * 2
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        mipmap: true
        cache: true
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.size * 0.08
        radius: root.size * 0.22
        color: Metrics.iconTile
        visible: image.status !== Image.Ready
    }
}
