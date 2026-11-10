pragma Singleton

import QtQuick

// The line icons Remote's boards draw that Wild OS's set doesn't have, as
// path data on the same 24px grid, for UI.Glyph and UI.IconButton's glyph.
QtObject {
    readonly property string key: "M15 9a4 4 0 1 1-3.9 4.9L4 21H2v-3l1-1h2v-2h2l2.1-2.1A4 4 0 0 1 15 9zM16.5 7.5h.01"
    readonly property string snippets: "M8 7l-5 5 5 5M16 7l5 5-5 5"
    readonly property string forward: "M4 12h13M13 7l5 5-5 5M20 5v14"
    readonly property string knownHosts: "M12 3l8 3v6c0 4.5-3.5 8-8 9-4.5-1-8-4.5-8-9V6z"
    readonly property string noSync: "M3 3l18 18M10.6 6.1A9.8 9.8 0 0 1 12 6c6 0 9.5 6 9.5 6a17 17 0 0 1-3 3.6M6.3 7.6A16.6 16.6 0 0 0 2.5 12S6 18 12 18a9 9 0 0 0 4-.9M9.9 10a3 3 0 0 0 4.2 4.2"
    readonly property string broadcast: "M12 12h.01M8.5 8.5a5 5 0 0 0 0 7M15.5 8.5a5 5 0 0 1 0 7M5.6 5.6a9 9 0 0 0 0 12.8M18.4 5.6a9 9 0 0 1 0 12.8"
    readonly property string columns: "M3 4h18v16H3zM12 4v16"
    readonly property string folder: "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"
    readonly property string clock: "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM12 7v5l3 2"
    readonly property string play: "M8 5l11 7-11 7z"
    readonly property string terminalTile: "M4 5h16v14H4zM8 10l2 2-2 2M12 14h4"
    readonly property string securityKey: "M9 3h6v6H9zM7 9h10v9a3 3 0 0 1-3 3h-4a3 3 0 0 1-3-3z"
    readonly property string signal: "M5 19v-3M10 19v-6M15 19v-9M20 19V7"
    readonly property string upload: "M12 18V6M7 11l5-5 5 5M5 20h14"
    readonly property string download: "M12 6v12M7 13l5 5 5-5M5 4h14"
    readonly property string file: "M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8zM14 3v5h5"
    readonly property string keyboard: "M3 6h18v12H3zM7 10h.01M11 10h.01M15 10h.01M7 14h10"
    readonly property string clipboard: "M9 4h6v3H9zM9 5H6v15h12V5h-3"
    readonly property string displays: "M3 5h12v9H3zM9 14v3M6 17h6M17 9h4v8h-7"
    readonly property string exitFull: "M9 4v5H4M15 4v5h5M9 20v-5H4M15 20v-5h5"
    readonly property string power: "M12 3v8M6.3 6.3a8 8 0 1 0 11.4 0"
}
