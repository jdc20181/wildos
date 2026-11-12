import QtQuick
import WildOS.UI as UI

// The 6px bar on Home's Storage and Battery cards. UI.ProgressBar is always
// accent or sand; the battery's charge is drawn in the success colour.
Rectangle {
    id: bar

    property real value: 0
    property color fill: UI.Style.accent

    implicitHeight: 6
    radius: 3
    color: UI.Style.tone(0.10)

    Rectangle {
        width: Math.max(0, Math.min(1, bar.value)) * parent.width
        height: parent.height
        radius: 3
        color: bar.fill
    }
}
