import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A page's 26px title (AppBackup.dc "Overview") with a line under it.
ColumnLayout {
    id: heading

    property string title
    property string subtitle

    spacing: 4

    UI.Label {
        Layout.fillWidth: true
        text: heading.title
        size: 26
        font.weight: UI.Style.weightBold
        Accessible.role: Accessible.Heading
    }
    UI.Label {
        Layout.fillWidth: true
        visible: heading.subtitle.length > 0
        text: heading.subtitle
        size: 13
        color: UI.Style.inkMuted
        wrapMode: Text.Wrap
    }
}
