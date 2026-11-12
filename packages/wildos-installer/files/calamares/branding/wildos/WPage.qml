/* Page scaffold shared by the installer's QML pages: a title, a line of
 * explanation and the page's own content below. Calamares pads the page area
 * (see stylesheet.qss), so the page starts at its top-left corner. The page
 * itself is transparent: the glass sheet behind it shows through.
 */
import QtQuick
import QtQuick.Layouts

Item {
    id: page

    property string title: ""
    property string subtitle: ""
    // The space between the page's blocks, which the artboards vary by page.
    property int gap: 22
    // Some artboards start the title a little higher than the usual 36 px.
    property int topInset: 0
    default property alias content: body.data

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: page.topInset
        anchors.bottomMargin: 22
        spacing: page.gap

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: page.title
                color: "#eef1f8"
                font.family: "Albert Sans"
                font.pixelSize: 28
                font.weight: 550
                font.letterSpacing: -0.28
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                visible: text !== ""
                text: page.subtitle
                color: Qt.rgba(232 / 255, 236 / 255, 248 / 255, 0.8)
                font.family: "Albert Sans"
                font.pixelSize: 14
                lineHeightMode: Text.FixedHeight
                lineHeight: 21
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: page.gap
        }
    }
}
