import QtQuick

// The Wild OS logo tile from branding/logo, at the 30 px the welcome
// artboards give it. Image owns its implicit size, so the size is set on
// the item itself.
Image {
    width: 30
    height: 30
    source: Qt.resolvedUrl("../../images/wild-os-icon.svg")
    sourceSize: Qt.size(90, 90)
    smooth: true
    mipmap: true
}
