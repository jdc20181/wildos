import QtQuick
import QtQuick.Effects

// A glass surface for things that are not windows: flyouts, popovers,
// the taskbar pills. `level` picks the token (bar, panel, menu, dialog,
// window, deep) and its matching radius and shadow from the design system:
// bar r20 with shadow-bar, panel r20 (Start uses r22) with shadow-flyout,
// menu r14. The compositor's blur is the host's job (Plasma dialogs,
// WindowFrame); without it this is still the right tint.
Rectangle {
    id: glass

    property string level: "panel"
    property bool shadow: true
    property bool topHighlight: !Style.dark && (level === "bar" || level === "panel")

    radius: level === "bar" || level === "panel" ? Style.radiusXl : level === "dialog" ? Style.radiusDialog : Style.radiusLg
    color: level === "bar" ? Style.glassBar
         : level === "menu" ? Style.glassMenu
         : level === "dialog" ? Style.glassDialog
         : level === "window" ? Style.glassWindow
         : level === "deep" ? Style.glassWindowDeep
         : Style.glassPanel
    border.width: 1
    border.color: level === "menu" ? Style.borderMenu : Style.borderGlass

    layer.enabled: shadow
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: glass.level === "bar" ? Qt.rgba(Style.shadowWindow.r, Style.shadowWindow.g, Style.shadowWindow.b, Style.dark ? 0.35 : 0.14) : Style.shadowFlyout
        shadowVerticalOffset: glass.level === "bar" ? 16 : 30
        shadowBlur: 1.0
        blurMax: glass.level === "bar" ? 40 : 64
    }

    // The 1px inset highlight along the top of shadow-bar and shadow-flyout.
    Rectangle {
        visible: glass.topHighlight
        x: glass.radius
        width: parent.width - 2 * glass.radius
        y: 1
        height: 1
        color: Style.dark ? Qt.rgba(1, 1, 1, glass.level === "bar" ? 0.10 : 0.08) : Qt.rgba(1, 1, 1, 0.7)
    }
}
