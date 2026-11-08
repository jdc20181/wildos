import QtQuick

// A file's icon from the Wild OS theme, replaced by a preview of the
// picture, video or document once KIO has made one.
Item {
    id: icon

    property string iconName
    property url url
    property bool preview: false
    property var stamp: 0
    property real size: 20
    property bool faded: false

    implicitWidth: size
    implicitHeight: size
    opacity: faded ? 0.5 : 1

    Image {
        id: themed
        anchors.fill: parent
        visible: thumb.status !== Image.Ready
        sourceSize: Qt.size(icon.size, icon.size)
        source: icon.iconName.length > 0 ? "image://icon/" + encodeURIComponent(icon.iconName) : ""
        smooth: true
    }

    Image {
        id: thumb
        anchors.fill: parent
        visible: status === Image.Ready
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectFit
        sourceSize: Qt.size(Math.ceil(icon.size * 2), Math.ceil(icon.size * 2))
        source: icon.preview ? "image://thumb/" + icon.stamp + "/" + encodeURIComponent(icon.url.toString()) : ""
        smooth: true
        mipmap: true
    }
}
