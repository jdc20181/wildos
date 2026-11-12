pragma Singleton

import QtQuick

// The same panel as the security flyout and quick settings: 400 wide, 20 px
// padding inside a 1 px glass border, 18 px between sections, a 56 px band
// along the foot. Colours and text sizes come from WildOS.UI.
QtObject {
    readonly property int flyoutWidth: 400
    readonly property int glassBorder: 1
    readonly property int pad: 20
    readonly property int sectionGap: 18
    readonly property int footerHeight: 56
    // Plasma's popup frame is drawn with a 22 px corner.
    readonly property int frameRadius: 22
}
