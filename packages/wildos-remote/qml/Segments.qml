import QtQuick
import WildOS.UI as UI

// A choice of two or three, side by side in one sunken track, the chosen
// one raised (RemoteNew.dc: "Ask every time" or "Save in keyring", Window,
// Full screen or All monitors). Arrow keys move the choice.
Rectangle {
    id: segments

    property var options: []
    property string value
    signal chosen(string value)

    // From the labels, not the row, whose segments share out its width.
    implicitWidth: {
        let w = 8;
        for (const o of options) {
            w += metrics.advanceWidth(o.text) + 34;
        }
        return Math.ceil(w);
    }
    FontMetrics {
        id: metrics
        font.family: UI.Style.fontFamily
        font.pixelSize: UI.Style.px(14)
        font.weight: Font.DemiBold
    }
    implicitHeight: 38
    radius: 10
    color: UI.Style.surfaceSunken
    border.width: 1
    border.color: UI.Style.borderField
    activeFocusOnTab: true
    Accessible.role: Accessible.List

    Keys.onLeftPressed: step(-1)
    Keys.onRightPressed: step(1)
    function step(d) {
        const i = options.findIndex(o => o.value === value);
        const next = options[Math.max(0, Math.min(options.length - 1, i + d))];
        if (next) {
            chosen(next.value);
        }
    }

    Row {
        id: row
        anchors.fill: parent
        anchors.margins: 4
        spacing: 2
        Repeater {
            model: segments.options
            Rectangle {
                id: seg
                required property var modelData
                readonly property bool current: segments.value === modelData.value
                width: (row.width - row.spacing * (segments.options.length - 1)) / segments.options.length
                height: row.height
                radius: 7
                color: current ? UI.Style.tone(0.12) : segMouse.containsMouse ? UI.Style.surfaceHover : "transparent"
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.text
                Accessible.checked: current
                UI.Label {
                    id: segLabel
                    anchors.centerIn: parent
                    text: seg.modelData.text
                    size: 14
                    font.weight: seg.current ? Font.DemiBold : Font.Medium
                    color: seg.current ? UI.Style.ink : UI.Style.inkMuted
                }
                MouseArea {
                    id: segMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: segments.chosen(seg.modelData.value)
                }
            }
        }
    }
    UI.FocusRing {
        visible: segments.activeFocus
        targetRadius: 10
    }
}
