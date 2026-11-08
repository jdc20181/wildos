import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import WildOS.Desktop as Desktop

// The Wild OS desktop containment. Plasma draws the wallpaper behind it
// (the org.kde.image plugin set by the look-and-feel layout); everything in
// front, icons and menus, is Wild OS's own (WildOS.Desktop).
ContainmentItem {
    id: root

    Desktop.DesktopView {
        id: desktop
        anchors.fill: parent
        config: Plasmoid.configuration
        primary: Plasmoid.screen === 0
        backdrop: root
        area: root.availableScreenRect
            ? Qt.rect(root.availableScreenRect.x, root.availableScreenRect.y, root.availableScreenRect.width, root.availableScreenRect.height)
            : Qt.rect(0, 0, width, height)
    }

    // Anything that still asks Plasma to configure the desktop (its
    // Alt+D, Alt+S shortcut, scripts) lands on Personalisation in Wild OS
    // Settings instead of Plasma's Desktop and Wallpaper dialog.
    PlasmaCore.Action {
        id: configureAction
        text: "Personalise"
        icon.name: "preferences-desktop-wallpaper"
        onTriggered: Desktop.DeskOps.openSettings("personalisation")
    }

    Component.onCompleted: Plasmoid.setInternalAction("configure", configureAction)
}
