import QtQuick
import WildOS.Clipboard

// The panel window lives for the whole session; shortcuts only show and
// hide it, so the clipboard history behind it never stops listening.
QtObject {
    property Panel panel: Panel {}
}
