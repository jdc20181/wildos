import QtQuick
import QtQuick.Templates as T
import WildOS.Music
import WildOS.UI as UI

// The big round play and pause button (AppMusic.dc): a 40px white disc
// with a near black glyph in either scheme.
T.Button {
    id: control

    property real side: 40

    implicitWidth: side
    implicitHeight: side
    padding: 0
    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    opacity: enabled ? 1 : 0.5
    Accessible.name: Player.playing ? qsTr("Pause") : qsTr("Play")
    Accessible.role: Accessible.Button
    onClicked: Player.playPause()

    background: Rectangle {
        radius: width / 2
        color: MusicStyle.playDisc
        border.width: UI.Style.dark ? 0 : 1
        border.color: UI.Style.borderSubtle
        scale: control.down ? 0.94 : control.hovered ? 1.05 : 1
        Behavior on scale {
            NumberAnimation { duration: UI.Style.motionFast }
        }

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: parent.radius
        }
    }

    contentItem: Item {
        MusicIcon {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: Player.playing ? 0 : 1
            name: Player.playing ? "pause" : "play"
            size: control.side * 0.45
            strokeWidth: 2.4
            color: MusicStyle.playGlyph
        }
    }

    UI.Tooltip {
        text: control.Accessible.name
        visible: control.hovered && !control.down
    }
}
