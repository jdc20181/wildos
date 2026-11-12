pragma Singleton

import QtQuick

// Voice's own line icons, on the system set's 24px grid and stroke, taken
// from the board (AppVoice.dc). Anything not here comes from WildOS.UI Icons.
QtObject {
    readonly property var paths: ({
        "mic": "M12 2a3 3 0 0 0 -3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0 -3 -3zM19 10v2a7 7 0 0 1 -14 0v-2M12 19v3",
        "waveform": "M2 10v3M6 6v11M10 3v18M14 8v7M18 5v13M22 10v3",
        "flag": "M4 15s1 -1 4 -1s5 2 8 2s4 -1 4 -1V3s-1 1 -4 1s-5 -2 -8 -2s-4 1 -4 1zM4 22v-7",
        "pause": "M15 4h2a1 1 0 0 1 1 1v14a1 1 0 0 1 -1 1h-2a1 1 0 0 1 -1 -1V5a1 1 0 0 1 1 -1zM7 4h2a1 1 0 0 1 1 1v14a1 1 0 0 1 -1 1H7a1 1 0 0 1 -1 -1V5a1 1 0 0 1 1 -1z",
        "play": "M7 4.5v15a1 1 0 0 0 1.5 0.86l12 -7.5a1 1 0 0 0 0 -1.72l-12 -7.5A1 1 0 0 0 7 4.5z",
        "back": "M3 12a9 9 0 1 0 9 -9a9.75 9.75 0 0 0 -6.74 2.74L3 8M3 3v5h5",
        "forward": "M21 12a9 9 0 1 1 -9 -9c2.52 0 4.93 1 6.74 2.74L21 8M21 3v5h-5",
        "chevron-down": "M6 9l6 6l6 -6",
        "search": "M3 11a8 8 0 1 0 16 0a8 8 0 1 0 -16 0zM21 21l-4.3 -4.3",
        "shield-check": "M20 13c0 5 -3.5 7.5 -7.66 8.95a1 1 0 0 1 -0.67 -0.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1 -1c2 0 4.5 -1.2 6.24 -2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1zM9 12l2 2l4 -4",
        "plus": "M5 12h14M12 5v14"
    })
    // Drawn filled in their own colour rather than as a line.
    readonly property var filled: ({
        "play": true
    })
}
