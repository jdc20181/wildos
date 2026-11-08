pragma Singleton

import QtQuick

// Terminal's own layout measurements: the settings view is laid out as the
// Settings window is (Settings.dc.html). Colours, type sizes, radii and the
// title bar height come from WildOS.UI's Style; the terminal's colour
// scheme is separate and comes from Config.
QtObject {
    readonly property int navWidth: 300
    readonly property int contentLeft: 20
    readonly property int contentRight: 40
    readonly property int sectionGap: 22
}
