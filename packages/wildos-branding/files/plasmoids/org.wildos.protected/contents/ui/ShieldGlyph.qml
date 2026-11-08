import QtQuick
import WildOS.UI as UI

// The security shield with an optional mark inside: a tick when protected,
// "!" when something needs doing. The marks are separate shapes shown and
// hidden rather than one path rewritten, because a Shape does not always
// redraw when its path text changes after it first appears.
Item {
    id: shield

    // "tick", "bang" or "" for the plain shield.
    property string mark: ""
    property color color: UI.Style.accent
    property real strokeWidth: 2

    implicitWidth: 18
    implicitHeight: 18

    UI.Glyph {
        anchors.fill: parent
        path: Icons.shield
        color: shield.color
        strokeWidth: shield.strokeWidth
    }

    UI.Glyph {
        anchors.fill: parent
        visible: shield.mark === "tick"
        path: Icons.tick
        color: shield.color
        strokeWidth: shield.strokeWidth
    }

    UI.Glyph {
        anchors.fill: parent
        visible: shield.mark === "bang"
        path: Icons.bang
        color: shield.color
        strokeWidth: shield.strokeWidth
    }
}
