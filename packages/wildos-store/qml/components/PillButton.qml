import QtQuick
import QtQuick.Templates as T
import WildOS.UI as UI

// The boards' pill for actions that are not an app's own button ("Update
// all", "Windows app settings"): 28px with accent words on a light tint,
// or `big` and `filled` in the accent.
T.Button {
    id: control

    property bool big: false
    property bool filled: false

    implicitWidth: Math.max(big ? 84 : 66, Math.ceil(label.implicitWidth) + (big ? 44 : 32))
    implicitHeight: Math.max(big ? 34 : 28, Math.ceil(label.implicitHeight) + 10)
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text

    background: Rectangle {
        radius: height / 2
        color: !control.enabled ? Metrics.buttonFill
            : control.filled ? (control.down ? UI.Style.accentPressed : control.hovered ? UI.Style.accentHover : UI.Style.accent)
            : control.down ? Metrics.buttonPressed : control.hovered ? Metrics.buttonHover : Metrics.buttonFill
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        UI.FocusRing {
            visible: control.visualFocus
            targetRadius: parent.radius
        }
    }

    contentItem: UI.Label {
        id: label
        horizontalAlignment: Text.AlignHCenter
        text: control.text
        size: control.big ? 14 : 13
        font.weight: Font.Bold
        color: !control.enabled ? Metrics.inkDim : control.filled ? UI.Style.textOnAccent : UI.Style.accent
    }
}
