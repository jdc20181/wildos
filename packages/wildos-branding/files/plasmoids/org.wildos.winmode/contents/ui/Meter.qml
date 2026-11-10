import QtQuick
import WildOS.UI as UI

// The 6 px memory meter from board 4.14: the accent on a faint track, paler
// while Windows is paused and the memory is only held.
Rectangle {
    id: meter

    property real value: 0
    property bool held: false

    implicitHeight: 6
    radius: 3
    color: UI.Style.tone(0.12)
    Accessible.role: Accessible.ProgressBar

    Rectangle {
        width: Math.round(meter.width * Math.max(0, Math.min(1, meter.value)))
        height: parent.height
        radius: 3
        color: meter.held ? Qt.rgba(UI.Style.accent.r, UI.Style.accent.g, UI.Style.accent.b, 0.6) : UI.Style.accent
        visible: width > 0
    }
}
