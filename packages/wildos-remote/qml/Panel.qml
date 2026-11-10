import QtQuick
import QtQuick.Effects
import QtQuick.Templates as T
import QtQuick.Window
import WildOS.UI as UI

// A dialog of Remote's own size and layout, on the Wild OS dialog glass
// (radius 18, the dialog glass and edge, the menu shadow, the scrim
// behind). For the forms and questions the boards draw larger than
// UI.Dialog's two sizes.
T.Popup {
    id: control

    parent: T.Overlay.overlay
    anchors.centerIn: parent
    // The templates leave a popup's size to the style; this one has none.
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    modal: true
    focus: true
    padding: 28
    closePolicy: T.Popup.CloseOnEscape

    T.Overlay.modal: Rectangle {
        radius: Window.window && Window.window.visibility !== Window.Maximized && Window.window.visibility !== Window.FullScreen ? UI.Style.windowRadius : 0
        color: UI.Style.scrim
    }

    background: Rectangle {
        radius: UI.Style.radiusDialog
        // Nearly solid: a dialog inside the window gets no blur from the
        // compositor, and the hosts behind would show through the glass.
        color: Qt.rgba(UI.Style.glassDialog.r, UI.Style.glassDialog.g, UI.Style.glassDialog.b, 0.98)
        border.width: 1
        border.color: UI.Style.borderDialog
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: UI.Style.shadowMenu
            shadowVerticalOffset: 24
            shadowBlur: 1.0
            blurMax: 48
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: UI.Style.motionFast; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: UI.Style.scaleEnter; to: 1; duration: UI.Style.motionNormal; easing.type: UI.Style.easeEnter }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: UI.Style.motionFast; easing.type: UI.Style.easeExit }
    }
}
