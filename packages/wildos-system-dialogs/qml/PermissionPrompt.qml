import QtQuick
import QtQuick.Window
import WildOS.UI as UI
import WildOS.SystemDialogs
import WildOS.SystemDialogs.Backend

// The permission prompt on the desktop: every screen dims and blurs
// (DialogPermission.dc rgba(12,14,18,0.55) with an 8px blur) under a layer
// surface above every window, which holds the keyboard until Yes or No.
// Apps cannot draw over it or put up a look-alike, because only the
// shell may make surfaces like this one.
Window {
    id: window

    visible: false
    color: "transparent"
    title: "Allow changes"

    Surface {
        id: surface
        window: window
        role: "overlay"
        scope: "wildos-permission"
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(UI.Style.scrim.r, UI.Style.scrim.g, UI.Style.scrim.b, surface.blurAvailable ? 0.55 : 0.8)
    }

    // Clicks on the dim do nothing, as on Windows.
    MouseArea {
        anchors.fill: parent
    }

    PermissionCard {
        id: card
        x: Math.round((window.width - width) / 2)
        // Where the board puts it (190 of 900); it grows downwards when the
        // details open rather than jumping.
        y: Math.max(24, Math.min(Math.round(window.height * 0.21), window.height - height - 24))
        height: implicitHeight
    }

    Instantiator {
        model: Qt.application.screens
        delegate: Backdrop {
            required property var modelData
            screen: modelData
            open: Agent.active && modelData.name !== window.screen.name
        }
    }

    Connections {
        target: Agent
        function onActiveChanged() {
            if (Agent.active) {
                surface.show();
                card.takeFocus();
            } else {
                surface.hide();
            }
        }
    }

    Component.onCompleted: {
        if (Agent.active) {
            surface.show();
            card.takeFocus();
        }
    }
}
