import QtQuick
import QtQuick.Templates as T

// The tabs of a title bar: TabButtons share the room left of the window
// buttons (72 to 220 each, 4 apart) with a 32px + after the last, which
// emits newTabRequested. From wildos-files TitleBar. Sits on the bottom of
// a 42px TitleBar so the current tab joins the bar below it.
T.TabBar {
    id: control

    property bool showNewTab: true
    property string newTabTip: "New tab (Ctrl+T)"
    property real room: width
    readonly property real tabWidth: Math.max(72, Math.min(220,
        (room - (showNewTab ? 40 : 0) - (count - 1) * spacing) / Math.max(1, count)))

    signal newTabRequested()

    implicitWidth: count * tabWidth + (count - 1) * spacing + (showNewTab ? 40 : 0)
    implicitHeight: 34
    spacing: 4
    padding: 0

    contentItem: ListView {
        model: control.contentModel
        currentIndex: control.currentIndex
        orientation: ListView.Horizontal
        spacing: control.spacing
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        implicitWidth: contentWidth
    }

    background: Item {
        IconButton {
            visible: control.showNewTab
            x: control.count * (control.tabWidth + control.spacing)
            y: control.height - height - 2
            width: 32
            height: 32
            iconName: "plus"
            iconSize: 14
            stroke: 2
            tip: control.newTabTip
            onClicked: control.newTabRequested()
        }
    }
}
