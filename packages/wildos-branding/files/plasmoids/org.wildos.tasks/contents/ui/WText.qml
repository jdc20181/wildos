import QtQuick

Text {
    color: Style.ink
    font.family: Style.fontFamily
    font.letterSpacing: Style.tracking(Style.px(13))
    font.pixelSize: Style.px(13)
    textFormat: Text.PlainText
    elide: Text.ElideRight
    maximumLineCount: 1
    verticalAlignment: Text.AlignVCenter
}
