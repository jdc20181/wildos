pragma Singleton

import QtQuick

// The clock flyout's measurements from the Flyouts artboard: notifications
// and calendar cards 380 wide with 16 px padding (18 at the calendar's sides
// and foot) inside a 1 px glass border, 12 px apart. Colours and text sizes
// come from WildOS.UI.
QtObject {
    readonly property int cardWidth: 380
    readonly property int glassBorder: 1
    readonly property int cardGap: 12
    readonly property int itemRadius: 14
    // Plasma's popup frame is drawn with a 22 px corner.
    readonly property int frameRadius: 22
}
