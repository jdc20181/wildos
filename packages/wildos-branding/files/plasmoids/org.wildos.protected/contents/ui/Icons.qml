pragma Singleton

import QtQuick

// The design's shield and the two marks drawn inside it, kept apart so the
// chip can show and hide a mark without rewriting a path (see ShieldGlyph).
QtObject {
    readonly property string shield: "M12 3l7 3v6c0 4.4-3 7.6-7 9-4-1.4-7-4.6-7-9V6z"
    readonly property string tick: "M9 12l2 2 4-4"
    readonly property string bang: "M12 8v4.5M12 15.8v.1"
}
