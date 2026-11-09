pragma Singleton

import QtQuick

// Every Wild OS line icon on the design's 24px grid, as SVG path data: the
// union of the sets in Files, Settings, Store, Welcome, App bridge and the
// quick settings flyout. Where two packages drew the same name differently
// the Files or Settings shape keeps the name and the other gets its own
// (check / checkmark, document / document-lines, list / list-bullets,
// back / chevron-left). Rules: 1.8 stroke (2 at 16px and below), round
// caps and joins, nothing filled except the names in `filled`.
QtObject {
    function r(x, y, w, h, rad) {
        return "M" + (x + rad) + " " + y + "H" + (x + w - rad)
            + "A" + rad + " " + rad + " 0 0 1 " + (x + w) + " " + (y + rad)
            + "V" + (y + h - rad)
            + "A" + rad + " " + rad + " 0 0 1 " + (x + w - rad) + " " + (y + h)
            + "H" + (x + rad)
            + "A" + rad + " " + rad + " 0 0 1 " + x + " " + (y + h - rad)
            + "V" + (y + rad)
            + "A" + rad + " " + rad + " 0 0 1 " + (x + rad) + " " + y + "Z";
    }

    function c(cx, cy, rad) {
        return "M" + (cx - rad) + " " + cy
            + "A" + rad + " " + rad + " 0 1 0 " + (cx + rad) + " " + cy
            + "A" + rad + " " + rad + " 0 1 0 " + (cx - rad) + " " + cy + "Z";
    }

    function has(name) {
        return paths[name] !== undefined;
    }

    readonly property var filled: ({ "more": true, "more-vertical": true, "avatar": true })

    readonly property var paths: ({
        "home": "M4 11l8-7 8 7v9H4z",
        "desktop": r(3, 4, 18, 13, 2) + "M8 21h8M12 17v4",
        "system": r(3, 4, 18, 13, 2) + "M8 21h8M12 17v4",
        "display": r(3, 4, 18, 12, 2) + "M9 20h6M12 16v4",
        "computer": r(3, 6, 18, 12, 2) + "M7 14h2",
        "document": "M6 3h9l4 4v14H6zM14 3v5h5",
        "document-lines": "M6 3h8l4 4v14H6zM14 3v4h4M9 12h6M9 16h6",
        "text-document": "M6 3h9l4 4v14H6zM14 3v5h5M9 13h6M9 17h6",
        "download": "M12 4v11M7 10l5 5 5-5M5 20h14",
        "installed": "M12 4v11M7 10l5 5 5-5M5 20h14",
        "pictures": r(3, 4, 18, 16, 2) + "M3 16l5-5 5 5 3-3 5 5",
        "image": r(3, 4, 18, 16, 2) + c(9, 9.5, 1.6) + "M21 16l-5-5-9 9",
        "music": "M9 18V5l11-2v13" + c(6.5, 18, 2.5) + c(17.5, 16, 2.5),
        "videos": r(3, 6, 13, 12, 2) + "M16 10.5l5-3v9l-5-3z",
        "video": r(3, 6, 13, 12, 2) + "M16 10.5l5-3v9l-5-3z",
        "camera": r(3, 7, 13, 11, 2) + "M16 11l5-3v9l-5-3z",
        "usb": r(7, 8, 10, 13, 2) + "M9 8V3h6v5",
        "drive": r(3, 7, 18, 10, 2) + "M7 12h5" + c(17, 12, 0.4),
        "storage": r(3, 4.5, 18, 6.5, 1.8) + r(3, 13, 18, 6.5, 1.8) + c(7, 7.75, 0.4) + c(7, 16.25, 0.4),
        "bin": "M4 7h16M6 7l1 13h10l1-13M9 7V4h6v3",
        "bin-full": "M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3",
        "delete": "M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3",
        "trash": "M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13",
        "folder": "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z",
        // Start's own: "Open file location", pin and unpin, "Sign out".
        "open-folder": "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2zM12 11v5M9.5 13.5L12 16l2.5-2.5",
        "pin": "M12 17v5M8 3h8l-1 7 3 3v2H6v-2l3-3z",
        "unpin": "M12 17v5M8 3h8l-1 7 3 3v2H6v-2l3-3zM3 3l18 18",
        "sign-out": "M15 4h4v16h-4M10 8l-4 4 4 4M6 12h10",
        "new-folder": "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2zM12 10v6M9 13h6",

        "back": "M19 12H5M11 6l-6 6 6 6",
        "forward": "M5 12h14M13 6l6 6-6 6",
        "arrow": "M5 12h14M13 6l6 6-6 6",
        "up": "M12 19V5M6 11l6-6 6 6",
        "refresh": "M20 12a8 8 0 1 1-2.3-5.7M20 4v5h-5",
        "update": "M20 12a8 8 0 1 1-2.3-5.7M20 4v5h-5",
        "restore": "M4 12a8 8 0 1 0 2.3-5.7M4 4v5h5",
        "rotate": "M4 12a8 8 0 0 1 13.7-5.7M20 4v5h-5M20 12a8 8 0 0 1-13.7 5.7M4 20v-5h5",
        "swap": "M4 12h16M14 6l6 6-6 6",
        "chevron": "M9 6l6 6-6 6",
        "chevron-right": "M9 6l6 6-6 6",
        "chevron-left": "M15 6l-6 6 6 6",
        "chevron-down": "M6 9l6 6 6-6",
        "chevron-up": "M6 15l6-6 6 6",
        "external": "M14 4h6v6M20 4l-9 9M18 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h5",
        "open": "M14 4h6v6M20 4l-9 9M18 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h5",

        "search": c(11, 11, 7) + "M20 20l-3.5-3.5",
        "plus": "M12 5v14M5 12h14",
        "minus": "M5 12h14",
        "new": c(12, 12, 9) + "M12 8v8M8 12h8",
        "close": "M6 6l12 12M18 6L6 18",
        "cross": "M7.5 7.5l9 9M16.5 7.5l-9 9",
        "check": "M5 12.5l4.5 4.5L19 7.5",
        "checkmark": "M5 12l5 5 9-10",
        "more": c(5, 12, 1.8) + c(12, 12, 1.8) + c(19, 12, 1.8),
        "more-vertical": c(12, 5, 1.8) + c(12, 12, 1.8) + c(12, 19, 1.8),
        "cut": c(6, 18, 3) + c(18, 18, 3) + "M8.2 16L18 3M15.8 16L6 3",
        "copy": r(8, 8, 12, 13, 2) + "M16 8V5a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h2",
        "paste": r(5, 5, 14, 16, 2) + r(9, 3, 6, 4, 1),
        "rename": r(3, 7, 14, 10, 2) + "M20 4v16M18 4h4M18 20h4",
        "pencil": "M4 20h4L19 9l-4-4L4 16zM13.5 6.5l4 4",
        "share": "M12 15V3M7 8l5-5 5 5M5 13v6a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-6",
        "link": "M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1",
        // The Files context menu (DialogFileMenu.dc.html).
        "zip": r(5, 3, 14, 18, 2) + "M12 3v2M12 7v2M12 11v2M10 15h4v3h-4z",
        "send": "M4 12l16-8-6 16-3-7z",
        "properties": r(4, 4, 16, 16, 2) + "M8 9h8M8 13h8M8 17h5",
        "sort": "M7 4v16M3 16l4 4 4-4M17 20V4M13 8l4-4 4 4",
        "view": r(3, 3, 7, 7, 1.5) + r(14, 3, 7, 7, 1.5) + r(3, 14, 7, 7, 1.5) + r(14, 14, 7, 7, 1.5),
        "apps": r(3, 3, 7.5, 7.5, 2) + r(13.5, 3, 7.5, 7.5, 2) + r(3, 13.5, 7.5, 7.5, 2) + r(13.5, 13.5, 7.5, 7.5, 2),
        "open-with": r(3, 3, 7.5, 7.5, 2) + r(13.5, 3, 7.5, 7.5, 2) + r(3, 13.5, 7.5, 7.5, 2) + r(13.5, 13.5, 7.5, 7.5, 2),
        "pane": r(3, 4, 18, 16, 2) + "M15 4v16",
        "list": "M4 6h16M4 12h16M4 18h16",
        "list-bullets": "M9 6h12M9 12h12M9 18h12" + c(4.5, 6, 0.9) + c(4.5, 12, 0.9) + c(4.5, 18, 0.9),
        "grid": r(4, 4, 6, 6, 1) + r(14, 4, 6, 6, 1) + r(4, 14, 6, 6, 1) + r(14, 14, 6, 6, 1),
        "large": r(4, 4, 16, 16, 2) + "M4 15l4.5-4.5 4 4 2.5-2.5 5 5",
        // The eight Files layouts, as in the File Explorer View menu.
        "view-xlarge": r(3, 3, 18, 18, 3),
        "view-large": r(3, 3, 8, 8, 2) + r(13, 3, 8, 8, 2) + r(3, 13, 8, 8, 2) + r(13, 13, 8, 8, 2),
        "view-medium": r(3, 3, 4.5, 4.5, 1) + r(9.75, 3, 4.5, 4.5, 1) + r(16.5, 3, 4.5, 4.5, 1)
            + r(3, 9.75, 4.5, 4.5, 1) + r(9.75, 9.75, 4.5, 4.5, 1) + r(16.5, 9.75, 4.5, 4.5, 1)
            + r(3, 16.5, 4.5, 4.5, 1) + r(9.75, 16.5, 4.5, 4.5, 1) + r(16.5, 16.5, 4.5, 4.5, 1),
        "view-small": r(3, 4.5, 3, 3, 0.8) + "M8.5 6h3" + r(14, 4.5, 3, 3, 0.8) + "M19.5 6h1.5"
            + r(3, 10.5, 3, 3, 0.8) + "M8.5 12h3" + r(14, 10.5, 3, 3, 0.8) + "M19.5 12h1.5"
            + r(3, 16.5, 3, 3, 0.8) + "M8.5 18h3" + r(14, 16.5, 3, 3, 0.8) + "M19.5 18h1.5",
        "view-list": r(3, 4.5, 3, 3, 0.8) + "M9 6h12" + r(3, 10.5, 3, 3, 0.8) + "M9 12h12" + r(3, 16.5, 3, 3, 0.8) + "M9 18h12",
        "view-tiles": r(3, 3.5, 7, 7, 1.5) + "M13 5.5h8M13 8.5h5" + r(3, 13.5, 7, 7, 1.5) + "M13 15.5h8M13 18.5h5",
        "view-content": r(3, 4, 5, 5, 1) + "M11 5h10M11 8h6M3 12h18" + r(3, 15, 5, 5, 1) + "M11 16h10M11 19h6",
        "select-all": r(4, 4, 16, 16, 2) + "M8 12l3 3 5-6",
        "select-none": r(4, 4, 16, 16, 2),
        "invert": r(4, 4, 16, 16, 2) + "M4 20L20 4",
        "new-tab": r(3, 5, 18, 15, 2) + "M3 9h18M12 12v5M9.5 14.5h5",
        "new-window": r(3, 4, 14, 12, 2) + "M7 20h12a2 2 0 0 0 2-2V8",
        "extension": "M6 3h9l4 4v14H6zM14 3v5h5M9 15l-2 2 2 2M15 15l2 2-2 2",
        "hidden": "M3 12s3.5-6 9-6 9 6 9 6-3.5 6-9 6-9-6-9-6z" + c(12, 12, 2.5),
        "eye": "M3 12s3.5-6 9-6 9 6 9 6-3.5 6-9 6-9-6-9-6z" + c(12, 12, 2.5),
        "eye-off": "M3 12s3.5-6 9-6c1.6 0 3 .5 4.2 1.2M21 12s-3.5 6-9 6c-1.6 0-3-.5-4.2-1.2M3 3l18 18",
        "app": r(4, 4, 16, 16, 4),
        "eject": "M12 5l7 8H5zM5 18h14",
        "terminal": r(3, 4, 18, 16, 2) + "M7 9l3 3-3 3M12 15h5",
        "box": "M12 3l8 4.5v9L12 21l-8-4.5v-9zM4 7.5l8 4.5 8-4.5M12 12v9",
        "sandbox": r(3, 4, 18, 16, 2) + "M3 9h18" + r(8, 12, 8, 5, 1),

        "shield": "M12 3l7 3v6c0 4.4-3 7.6-7 9-4-1.4-7-4.6-7-9V6z",
        "shield-check": "M12 3l7 3v6c0 4.4-3 7.6-7 9-4-1.4-7-4.6-7-9V6zM9 12l2 2 4-4",
        "shield-alert": "M12 3l7 3v6c0 4.4-3 7.6-7 9-4-1.4-7-4.6-7-9V6zM12 8.5v4" + c(12, 15.6, 0.35),
        "vpn": "M12 3l7 3v6c0 4.4-3 7.6-7 9-4-1.4-7-4.6-7-9V6zM9.5 12h5",
        "lock": r(5, 11, 14, 10, 2) + "M8 11V8a4 4 0 0 1 8 0v3",
        "firewall": r(3, 4, 18, 16, 2) + "M3 9.5h18M3 15h18M9 4v5.5M15 9.5V15M9 15v5",
        "info": c(12, 12, 9) + "M12 11v5.5" + c(12, 7.8, 0.35),
        "alert": c(12, 12, 9) + "M12 7.5v5.5" + c(12, 16.4, 0.35),
        "warning": "M12 3.5l9.5 16.5h-19zM12 10v4.5" + c(12, 17.3, 0.35),
        // The curve renderer drops a zero-length "h0" stroke, so dots are
        // tiny circles.
        "question": "M9.5 9.5a2.5 2.5 0 1 1 3.5 2.3c-.6.3-1 .9-1 1.6v.6" + c(12, 17, 0.35),

        "bluetooth": "M7 7l10 10-5 5V2l5 5L7 17",
        "wifi": "M2.5 9a14 14 0 0 1 19 0M5.5 12.5a9.5 9.5 0 0 1 13 0M8.6 16a5 5 0 0 1 6.8 0" + c(12, 19.5, 0.6),
        "wifi-low": "M8.6 16a5 5 0 0 1 6.8 0" + c(12, 19.5, 0.6),
        "wifi-mid": "M5.5 12.5a9.5 9.5 0 0 1 13 0M8.6 16a5 5 0 0 1 6.8 0" + c(12, 19.5, 0.6),
        "wifi-off": "M2.5 9a14 14 0 0 1 4.5-2.9M11 5.1a14 14 0 0 1 10.5 3.9M5.5 12.5a9.5 9.5 0 0 1 4-2.3M14.8 10.5a9.5 9.5 0 0 1 3.7 2M8.6 16a5 5 0 0 1 6.8 0M3 3l18 18" + c(12, 19.5, 0.6),
        "ethernet": r(4, 4, 16, 13, 2) + "M8 17v3h8v-3M9 8.5v3M12 8.5v3M15 8.5v3",
        "airplane": "M10.5 3.5c0-1 .7-1.5 1.5-1.5s1.5.5 1.5 1.5V9l7 4v2l-7-2v4.5l2 1.5v1.5l-3.5-1-3.5 1V19l2-1.5V13l-7 2v-2l7-4z",
        "globe": c(12, 12, 9) + "M3 12h18M12 3a14 14 0 0 1 0 18M12 3a14 14 0 0 0 0 18",
        "sound": "M4 9.5h3.5L12 5v14l-4.5-4.5H4zM15.5 9a4 4 0 0 1 0 6M18 6.5a7.5 7.5 0 0 1 0 11",
        "sound-off": "M4 9.5h3.5L12 5v14l-4.5-4.5H4zM16 9.5l5 5M21 9.5l-5 5",
        "mic": r(9, 3, 6, 11, 3) + "M5 11a7 7 0 0 0 14 0M12 18v3",
        "headphones": "M4 15v-3a8 8 0 0 1 16 0v3" + r(3, 14, 4, 7, 1.5) + r(17, 14, 4, 7, 1.5),
        "bell": "M6 16v-5a6 6 0 0 1 12 0v5l1.5 2h-15zM10 20.5a2 2 0 0 0 4 0",
        "battery": r(2.5, 7, 17, 10, 2) + "M22 10.5v3M6.5 10.5v3M10 10.5v3",
        "plug": "M8 3v5M16 3v5M6 8h12v3a6 6 0 0 1-12 0zM12 17v4",
        "power": "M12 3v8M7.5 6.5a7 7 0 1 0 9 0",
        "startup": "M12 3v8M7.5 6.5a7 7 0 1 0 9 0",
        "moon": "M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z",
        // Lock, sign-in and power screens.
        "restart": "M20 12a8 8 0 1 1-2.3-5.7M20 4v5h-5",
        "hibernate": r(4, 4, 16, 16, 3) + "M9 9.5h6l-6 5h6",
        "accessibility": c(12, 4.5, 1.8) + "M5 8.5l7 1.5 7-1.5M12 10v4.5M12 14.5l-3.5 6M12 14.5l3.5 6",
        "caps-lock": "M12 4l7 7h-4v5H9v-5H5zM9 20h6",
        "task-manager": "M3 12h4l3-7 4 14 3-7h4",
        // Task Manager's pages (TaskManager.dc navigation).
        "processes": r(4, 4, 16, 16, 2) + "M8 9h8M8 13h8M8 17h5",
        "performance": "M3 17l5-6 4 3 4-7 5 6",
        "startup-apps": "M12 3v9M6.3 6.8a8 8 0 1 0 11.4 0",
        "monitor": r(3, 4, 18, 13, 2) + "M8 21h8",
        "services": c(12, 12, 3) + "M12 3v3M12 18v3M3 12h3M18 12h3M5.6 5.6l2.1 2.1M16.3 16.3l2.1 2.1M5.6 18.4l2.1-2.1M16.3 7.7l2.1-2.1",
        "options": c(12, 12, 3) + "M12 2v3M12 19v3M2 12h3M19 12h3",
        "play": "M8 5.5v13l10.5-6.5z",
        "switch-user": c(9, 8, 3.5) + "M2.5 20c1-3.5 3.6-5.5 6.5-5.5 1.6 0 3 .5 4.2 1.5M16 14h6M19.5 11.5L22 14l-2.5 2.5M22 19h-6M18.5 16.5L16 19l2.5 2.5",
        "sun": c(12, 12, 4) + "M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4",
        "mouse": r(6, 3, 12, 18, 6) + "M12 7v3",
        "touchpad": r(3, 4, 18, 16, 2) + "M3 15h18M12 15v5",
        "keyboard": r(2.5, 6, 19, 12, 2) + c(6.5, 10, 0.35) + c(10, 10, 0.35) + c(14, 10, 0.35) + c(17.5, 10, 0.35) + "M8 14h8",
        "printer": "M7 9V3h10v6" + r(3, 9, 18, 8, 2) + "M7 14h10v7H7z",
        "phone": r(7, 2.5, 10, 19, 2.5) + "M11 18.5h2",
        "chip": r(6, 6, 12, 12, 2) + "M9 3v3M15 3v3M9 18v3M15 18v3M3 9h3M3 15h3M18 9h3M18 15h3",
        "resolution": r(3, 4, 18, 13, 2) + "M7 8V7.5h2.5M17 13v.5h-2.5M8 21h8",

        "palette": "M12 3a9 9 0 1 0 0 18c1.1 0 1.6-.8 1.2-1.7-.5-1 .2-2.3 1.4-2.3H17a4 4 0 0 0 4-4c0-5.5-4-10-9-10z" + c(7.5, 11, 1.2) + c(11, 7.5, 1.2) + c(15.5, 8.5, 1.2),
        "brush": "M19 4l-9 11M10 15c-2 0-3.5 1.5-3.5 3.5 0 .9-.6 1.5-1.5 1.5 3.5 1.5 7 0 7-3",
        "drop": "M12 3.5c3.5 4 6 7.2 6 10.5a6 6 0 0 1-12 0c0-3.3 2.5-6.5 6-10.5z",
        "font": "M3 19l5.5-14 5.5 14M5.2 14h6.6" + c(18, 16.2, 2.8) + "M20.8 13v6",
        "taskbar": r(3, 4, 18, 16, 2) + "M3 16h18M10 18h4",
        "multitasking": r(3, 4, 8, 16, 2) + r(13, 4, 8, 7, 2) + r(13, 13, 8, 7, 2),
        "desktops": r(3, 6, 13, 10, 2) + "M8 19.5h11a2 2 0 0 0 2-2V9",
        "windowsapps": r(3, 4, 18, 16, 2) + "M3 9h18M14 13.5l2 1v1.6c0 1.2-.8 2-2 2.4-1.2-.4-2-1.2-2-2.4v-1.6z",
        "default-apps": r(3, 3, 18, 18, 3) + "M8 12l3 3 5-6",
        "account": c(12, 8, 4) + "M4 21c1.5-4 4.5-6 8-6s6.5 2 8 6",
        "people": c(9, 8, 3.5) + "M2.5 20c1.2-3.4 3.6-5 6.5-5s5.3 1.6 6.5 5M15.5 4.8a3.5 3.5 0 0 1 0 6.4M17.5 15.3c1.9.7 3.2 2.2 4 4.7",
        "avatar": c(12, 8.5, 4.6) + "M2.5 24c0-5.6 4.3-9.6 9.5-9.6s9.5 4 9.5 9.6z",
        "clock": c(12, 12, 9) + "M12 7v5l3 2",
        "language": "M4 5h9M8.5 3v2M6 5c.5 3 3 6 6 7.5M11 5c-.5 3-3 6-6 7.5M13 21l4-9 4 9M14.5 18h5",
        "location": "M12 21s-7-6.2-7-12a7 7 0 0 1 14 0c0 5.8-7 12-7 12z" + c(12, 9, 2.5),
        "mail": r(3, 5, 18, 14, 2) + "M3.5 6.5l8.5 6.5 8.5-6.5",
        "store": "M5 8h14l-1 12H6zM9 8V6.5a3 3 0 0 1 6 0V8",
        "games": r(2, 7, 20, 11, 5) + "M7 11v3M5.5 12.5h3M15.5 12h0M18 13.5h0",
        // Terminal: profiles, panes and the profile icons a tab can carry.
        "pointer": "M5 3l14 7.5-6 1.5-3 6z",
        "profiles": r(3, 4, 18, 16, 2) + "M7 9l3 3-3 3M12 15h5",
        "split-right": r(3, 4, 18, 16, 2) + "M12 4v16",
        "split-down": r(3, 4, 18, 16, 2) + "M3 12h18",
        "close-pane": r(3, 4, 18, 16, 2) + "M9.5 9.5l5 5M14.5 9.5l-5 5",
        "code": "M8 7l-5 5 5 5M16 7l5 5-5 5M14 4l-4 16",
        "server": r(3, 4, 18, 7, 2) + r(3, 13, 18, 7, 2) + c(7, 7.5, 0.4) + c(7, 16.5, 0.4),
        "star": "M12 3.5l2.6 5.4 5.9.8-4.3 4.1 1 5.8L12 16.9l-5.2 2.7 1-5.8-4.3-4.1 5.9-.8z",
        "bolt": "M13 3L5 13.5h6L10 21l8-10.5h-6z",
        "duplicate": r(8, 8, 12, 12, 2) + "M16 8V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h2M14 11v6M11 14h6",
        "fullscreen": "M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5",
        // A bare exclamation mark for inside a round status badge (WinApp.dc),
        // where "alert" would draw a circle in a circle.
        "exclamation": "M12 6.5v7" + c(12, 17.4, 0.35),
        "cog": "M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39a2 2 0 0 0-.73-2.73l-.15-.08a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z" + c(12, 12, 3),

        // The window frame glyphs on the 12 unit box the Aurorae buttons
        // and the Settings title bar draw, doubled onto this grid.
        "win-minimise": "M3 12H21",
        "win-maximise": r(3, 3, 18, 18, 3.6),
        "win-restore": r(4.4, 7, 14, 14, 2.8) + "M8.8 3h9.8A3.6 3.6 0 0 1 22.2 6.6v9.8",
        "win-close": "M4 4L20 20M20 4L4 20",

        // The desktop's right-click menu (DesktopIcons.dc), drawn apart from
        // the Files command bar's View, Sort, Refresh and New.
        "desktop-view": r(4, 4, 7, 7, 1.5) + r(13, 4, 7, 7, 1.5) + r(4, 13, 7, 7, 1.5) + r(13, 13, 7, 7, 1.5),
        "desktop-sort": "M7 5v14M4 16l3 3 3-3M14 7h6M14 12h4M14 17h2",
        "desktop-refresh": "M20 11a8 8 0 1 0-2.3 5.7M20 5v6h-6",
        "desktop-personalise": "M4 20l4-1 10-10-3-3L5 16zM13 6l3 3",
        "desktop-terminal": "M5 8l4 4-4 4M11 16h8",

        // The right-click menu rows of MenusShell.dc and MenusApps.dc that
        // no other surface draws.
        "menu-apps": "M4 4h6v6H4zM14 4h6v6h-6zM4 14h6v6H4zM14 14h6v6h-6z",
        "menu-task-manager": r(3, 4, 18, 16, 2) + "M7 15l3-4 3 3 4-6",
        "menu-settings": c(12, 12, 3) + "M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M4.9 19.1L7 17M17 7l2.1-2.1",
        "menu-run": "M14 4h6v6M20 4l-9 9M18 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h5",
        "menu-power": "M12 3v9M6.3 6.3a8 8 0 1 0 11.4 0",
        "menu-desktop": r(3, 4, 18, 13, 2) + "M8 21h8",
        "menu-windows-mode": r(3, 4, 18, 13, 2) + "M8 21h8M12 17v4",
        "menu-paste": r(6, 4, 12, 17, 2) + "M9 4V3h6v1",
        "menu-sort": "M7 4v16M3 16l4 4 4-4M17 20V4M13 8l4-4 4 4",
        "menu-refresh": "M20 12a8 8 0 1 1-2.3-5.7M20 4v5h-5",
        "menu-personalise": "M14 4l6 6-8 8H6v-6zM4 20h4",
        "menu-pin": "M12 17v4M8 3h8l-1 6 3 3v2H6v-2l3-3z",
        "menu-unpin": "M12 17v4M8 3h8l-1 6 3 3v2H6v-2l3-3zM3 3l18 18",
        "menu-front": "M12 19V8M7 12l5-5 5 5M5 4h14",
        "menu-uninstall": "M4 7h16M10 11v6M14 11v6M6 7l1 12a2 2 0 0 0 2 2h6a2 2 0 0 0 2-2l1-12M9 7V4h6v3",
        "menu-maximise": r(5, 5, 14, 14, 2),
        "menu-snap": r(3, 4, 18, 16, 2) + "M12 4v16M12 12h9",
        "menu-move": "M12 3v18M3 12h18M8 7l4-4 4 4M8 17l4 4 4-4M7 8l-4 4 4 4M17 8l4 4-4 4",
        "menu-window": r(3, 4, 18, 16, 2) + "M3 9h18",
        "menu-document": "M5 6h14M12 6v13",
        "menu-cut": c(6, 18, 3) + c(18, 18, 3) + "M8 16L19 4M16 16L5 4",
        "menu-copy": r(9, 9, 11, 11, 2) + "M5 15V6a1 1 0 0 1 1-1h9",
        "undo": "M9 14L4 9l5-5M4 9h10a6 6 0 0 1 0 12h-3",
        "media-previous": "M7 6v12M18 6l-8 6 8 6z",
        "media-pause": "M8 5v14M16 5v14",
        "media-next": "M17 6v12M6 6l8 6-8 6z",
        "emoji": c(12, 12, 9) + "M8.5 14.5a4.5 4.5 0 0 0 7 0" + c(9, 9.5, 0.4) + c(15, 9.5, 0.4),
        "menu-volume-mixer": "M6 4v16M12 4v16M18 4v16" + r(4, 13, 4, 3, 1) + r(10, 7, 4, 3, 1) + r(16, 15, 4, 3, 1),
        "menu-output": "M4 15v-3a8 8 0 0 1 16 0v3" + r(3, 14, 4, 6, 1.5) + r(17, 14, 4, 6, 1.5)
    })
}
