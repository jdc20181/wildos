pragma Singleton

import QtQuick
import WildOS.UI as UI

// The status colours from board 4.13, dark then light: green while running,
// blue while paused or starting, grey once saved or off, sand while updating
// or when something needs attention. The word beside the dot always says
// the state too, so colour is never the only signal.
QtObject {
    readonly property bool dark: UI.Style.dark

    function dot(level: string): color {
        switch (level) {
        case "running":
            return dark ? "#7fe0ad" : "#1c9a62";
        case "paused":
        case "starting":
            return dark ? "#8fb4ff" : "#3a56c4";
        case "updating":
        case "attention":
            return dark ? "#f5c27a" : "#b7791f";
        }
        return dark ? Qt.rgba(232 / 255, 236 / 255, 248 / 255, 0.55) : Qt.rgba(20 / 255, 26 / 255, 46 / 255, 0.45);
    }

    // The panel's state pill: a soft fill of the dot's colour with lighter
    // text in dark mode (the Running and Paused pills on board 4.14).
    function pillText(level: string): color {
        switch (level) {
        case "running":
            return UI.Style.success;
        case "paused":
        case "starting":
            return UI.Style.accentText;
        case "updating":
        case "attention":
            return dot(level);
        }
        return UI.Style.inkMuted;
    }

    function pillFill(level: string): color {
        const c = dot(level);
        return level === "saved" || level === "off" ? UI.Style.tone(0.10) : Qt.rgba(c.r, c.g, c.b, dark ? 0.16 : 0.12);
    }

    // The taskbar behind the chip, for the ring that sets the dot off the
    // window mark.
    readonly property color bar: dark ? Qt.rgba(14 / 255, 18 / 255, 34 / 255, 0.88) : Qt.rgba(246 / 255, 248 / 255, 252 / 255, 0.92)
}
