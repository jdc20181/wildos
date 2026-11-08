pragma Singleton

import QtQuick
import WildOS.UI as UI

// The Performance page's resource colours (TaskManagerPerf.dc: the
// processor and Windows mode in the accent, memory violet, disks green,
// the network coral, graphics amber) and the Processes page's heat shading.
// Light mode deepens each hue so a thin line still reads on pale glass.
QtObject {
    readonly property bool dark: UI.Style.dark

    function resource(key) {
        switch (key) {
        case "memory":
            return dark ? "#b9a6f5" : "#6a4fcf";
        case "disk":
            return dark ? "#7fe0b0" : "#1d8a57";
        case "network":
            return dark ? "#eeb0a4" : "#c05440";
        case "gpu":
            return dark ? "#f5c27a" : "#a36508";
        default:
            return UI.Style.accent;
        }
    }

    function alpha(colour, a) {
        return Qt.rgba(colour.r, colour.g, colour.b, a);
    }

    // How strongly a cell is shaded, measured from TaskManager.dc: an idle
    // cell is clear, anything above zero starts at 0.05 and the shade grows
    // with use (6.2% CPU 0.15, 812 MB of 16 GB 0.21, 0.4 MB/s 0.10,
    // 0.8 Mbps 0.25), up to 0.6.
    function heat(kind, value, memoryTotal) {
        if (!(value > 0)) {
            return 0;
        }
        let a = 0.05;
        if (kind === "cpu") {
            a += 0.017 * value;
        } else if (kind === "memory") {
            a += 3.1 * value / Math.max(1, memoryTotal);
        } else if (kind === "disk") {
            a += 0.125 * value / 1048576;
        } else if (kind === "network") {
            a += 0.25 * value * 8 / 1000000;
        }
        return Math.min(0.6, a);
    }

    function heatColour(a) {
        return Qt.rgba(UI.Style.accent.r, UI.Style.accent.g, UI.Style.accent.b, dark ? a : a * 0.75);
    }
}
