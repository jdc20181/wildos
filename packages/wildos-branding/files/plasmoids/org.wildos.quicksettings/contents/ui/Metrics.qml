pragma Singleton

import QtQuick

// The quick settings panel's own measurements from the Flyouts artboard:
// 400 wide, 20 px padding inside a 1 px glass border, 18 px between
// sections, 76 px tiles, a 56 px battery band. Colours and text sizes come
// from WildOS.UI.
QtObject {
    readonly property int flyoutWidth: 400
    readonly property int glassBorder: 1
    readonly property int pad: 20
    readonly property int sectionGap: 18
    readonly property int tileButtonHeight: 76
    readonly property int tileGap: 8
    readonly property int tileRadius: 14
    readonly property int footerHeight: 56
    // Plasma's popup frame is drawn with a 22 px corner.
    readonly property int frameRadius: 22
}
