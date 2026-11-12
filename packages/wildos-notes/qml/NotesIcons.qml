pragma Singleton

import QtQuick

// Notes' own line icons, on the system set's 24px grid and stroke, taken
// from the board (AppNotes.dc). Anything not here comes from WildOS.UI Icons.
QtObject {
    readonly property var paths: ({
        "all-notes": "M2 6h4M2 10h4M2 14h4M2 18h4M6 2h12a2 2 0 0 1 2 2v16a2 2 0 0 1 -2 2H6a2 2 0 0 1 -2 -2V4a2 2 0 0 1 2 -2zM9.5 8h5M9.5 12H16M9.5 16H14",
        "pin": "M12 17v5M9 10.76a2 2 0 0 1 -1.11 1.79l-1.78 0.9A2 2 0 0 0 5 15.24V16a1 1 0 0 0 1 1h12a1 1 0 0 0 1 -1v-0.76a2 2 0 0 0 -1.11 -1.79l-1.78 -0.9A2 2 0 0 1 15 10.76V7a1 1 0 0 1 1 -1a2 2 0 0 0 0 -4H8a2 2 0 0 0 0 4a1 1 0 0 1 1 1z",
        "notebook": "M4 19.5v-15A2.5 2.5 0 0 1 6.5 2H19a1 1 0 0 1 1 1v18a1 1 0 0 1 -1 1H6.5a1 1 0 0 1 0 -5H20",
        "hash": "M4 9h16M4 15h16M10 3L8 21M16 3l-2 18",
        "plus": "M5 12h14M12 5v14",
        "keyboard": "M4 4h16a2 2 0 0 1 2 2v12a2 2 0 0 1 -2 2H4a2 2 0 0 1 -2 -2V6a2 2 0 0 1 2 -2zM6 8h0.01M10 8h0.01M14 8h0.01M18 8h0.01M8 12h0.01M12 12h0.01M16 12h0.01M7 16h10",
        "search": "M3 11a8 8 0 1 0 16 0a8 8 0 1 0 -16 0zM21 21l-4.3 -4.3",
        "sort": "M3 16l4 4l4 -4M7 20V4M11 4h10M11 8h7M11 12h4",
        "new-note": "M12 3H5a2 2 0 0 0 -2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2 -2v-7M18.375 2.625a1 1 0 0 1 3 3l-9.013 9.014a2 2 0 0 1 -0.853 0.505l-2.873 0.84a0.5 0.5 0 0 1 -0.62 -0.62l0.84 -2.873a2 2 0 0 1 0.506 -0.852z",
        "heading": "M4 12h8M4 18V6M12 18V6M17 12l3 -2v8",
        "bold": "M6 12h9a4 4 0 0 1 0 8H7a1 1 0 0 1 -1 -1V5a1 1 0 0 1 1 -1h7a4 4 0 0 1 0 8",
        "italic": "M19 4h-9M14 20H5M15 4L9 20",
        "underline": "M6 4v6a6 6 0 0 0 12 0V4M4 20h16",
        "bullets": "M3 6h0.01M3 12h0.01M3 18h0.01M8 6h13M8 12h13M8 18h13",
        "checklist": "M3 7l2 2l4 -4M3 17l2 2l4 -4M13 6h8M13 12h8M13 18h8",
        "link": "M10 13a5 5 0 0 0 7.54 0.54l3 -3a5 5 0 0 0 -7.07 -7.07l-1.72 1.71M14 11a5 5 0 0 0 -7.54 -0.54l-3 3a5 5 0 0 0 7.07 7.07l1.71 -1.71",
        "image": "M5 3h14a2 2 0 0 1 2 2v14a2 2 0 0 1 -2 2H5a2 2 0 0 1 -2 -2V5a2 2 0 0 1 2 -2zM7 9a2 2 0 1 0 4 0a2 2 0 1 0 -4 0zM21 15l-3.086 -3.086a2 2 0 0 0 -2.828 0L6 21",
        "table": "M5 3h14a2 2 0 0 1 2 2v14a2 2 0 0 1 -2 2H5a2 2 0 0 1 -2 -2V5a2 2 0 0 1 2 -2zM12 3v18M3 9h18M3 15h18",
        "share": "M4 12v8a2 2 0 0 0 2 2h12a2 2 0 0 0 2 -2v-8M16 6l-4 -4l-4 4M12 2v13",
        "dots": "M11 12a1 1 0 1 0 2 0a1 1 0 1 0 -2 0zM18 12a1 1 0 1 0 2 0a1 1 0 1 0 -2 0zM4 12a1 1 0 1 0 2 0a1 1 0 1 0 -2 0z",
        "open": "M15 3h6v6M10 14L21 3M18 13v6a2 2 0 0 1 -2 2H5a2 2 0 0 1 -2 -2V8a2 2 0 0 1 2 -2h6",
        "x": "M18 6L6 18M6 6l12 12"
    })
}
