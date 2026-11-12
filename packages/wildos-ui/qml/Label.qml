import QtQuick

// Text in Albert Sans at the design's UI size, scaled with the system text size.
// Set `size` to a design pixel size; it is scaled for you.
Text {
    property real size: 13

    color: Style.ink
    font.family: Style.fontFamily
    font.pixelSize: Style.px(size)
    // Read through a property of its own: a binding on font that reads
    // font.pixelSize runs again when it writes letterSpacing, which Qt
    // reports as a binding loop. This one only changes with the size.
    readonly property real trackedSize: font.pixelSize
    font.letterSpacing: Style.tracking(trackedSize)
    textFormat: Text.PlainText
    elide: Text.ElideRight
    maximumLineCount: wrapMode === Text.NoWrap ? 1 : 0
    verticalAlignment: Text.AlignVCenter
}
