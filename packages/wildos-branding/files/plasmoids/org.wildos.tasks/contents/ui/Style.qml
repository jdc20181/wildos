pragma Singleton

import QtQuick
import org.kde.kirigami as Kirigami

// Values from the Wild OS design tokens and the Taskbar artboard, kept in one
// place so the taskbar cannot drift from Start and the flyouts.
QtObject {
    readonly property string fontFamily: "Inter"
    // Settings > Personalisation > Text size sets the system font; Inter 10 pt
    // is the Wild OS default, so 10 pt draws text at the design's sizes.
    readonly property real textScale: Math.max(0.75, Kirigami.Theme.defaultFont.pointSize / 10)

    function px(size: real): int {
        return Math.round(size * textScale);
    }

    readonly property color ink: "#eef1f8"
    readonly property color inkMuted: Qt.rgba(232 / 255, 236 / 255, 248 / 255, 0.72)
    readonly property color inkSubtle: Qt.rgba(232 / 255, 236 / 255, 248 / 255, 0.58)
    readonly property color accent: "#8fb4ff"
    readonly property color textOnAccent: "#0f1530"
    readonly property color warning: "#f2c46b"

    readonly property color hover: Qt.rgba(1, 1, 1, 0.09)
    readonly property color pressed: Qt.rgba(1, 1, 1, 0.12)
    // The app in front sits on a soft highlight (LaunchStates, focused).
    readonly property color focused: Qt.rgba(1, 1, 1, 0.10)
    readonly property color focusedHover: Qt.rgba(1, 1, 1, 0.14)
    // Starting: a pale blue line and a faint glow that pulses with it.
    readonly property color startingMark: "#b9cfff"
    readonly property color startingGlow: Qt.rgba(1, 1, 1, 0.07)
    readonly property color attention: Qt.rgba(242 / 255, 196 / 255, 107 / 255, 0.22)
    readonly property color divider: Qt.rgba(1, 1, 1, 0.08)
    readonly property color menuGlass: Qt.rgba(22 / 255, 26 / 255, 46 / 255, 0.94)
    readonly property color menuBorder: Qt.rgba(1, 1, 1, 0.14)
    readonly property color menuShadow: Qt.rgba(4 / 255, 8 / 255, 24 / 255, 0.6)
    readonly property color badge: "#1b2038"
    readonly property color badgeHover: "#2a3150"
    readonly property color badgeBorder: Qt.rgba(1, 1, 1, 0.16)
    readonly property color progressTrack: Qt.rgba(1, 1, 1, 0.22)

    // The artboard's app buttons: 48 px square, 12 px corners, 4 px apart.
    // Icons are trimmed to their visible shape and drawn as tall as the Start
    // glyph's dots (16.6 of 24 units in a 30 px box), not the artboard's
    // 34 px, which towered over Start and Task view on the docked bar. The
    // running mark is 6 by 3 px, 2 px off the bottom; the focused one 18 px
    // (LaunchStates).
    readonly property int button: 48
    readonly property int buttonRadius: 12
    readonly property int icon: 21
    readonly property int gap: 4
    readonly property int markWidth: 6
    readonly property int markFocusedWidth: 18
    readonly property int startingWidth: 22
    readonly property int startingCycle: 1600
    readonly property int markHeight: 3
    readonly property int markBottom: 2
    readonly property color mark: Qt.rgba(1, 1, 1, 0.7)

    // The press dip: down to 0.86, springing past full size to 1.04.
    readonly property real dipScale: 0.86
    readonly property real dipOvershoot: 1.04

    readonly property int hoverDuration: 150
    readonly property int menuWidth: 280
}
