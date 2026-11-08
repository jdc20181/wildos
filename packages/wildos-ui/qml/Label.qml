import QtQuick

// Text in Inter at the design's UI size, scaled with the system text size.
// Set `size` to a design pixel size; it is scaled for you.
Text {
    property real size: 13

    color: Style.ink
    font.family: Style.fontFamily
    font.pixelSize: Style.px(size)
    textFormat: Text.PlainText
    elide: Text.ElideRight
    maximumLineCount: wrapMode === Text.NoWrap ? 1 : 0
    verticalAlignment: Text.AlignVCenter
}
