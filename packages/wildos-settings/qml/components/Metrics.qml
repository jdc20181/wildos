pragma Singleton

import QtQuick

// The Settings window's own layout measurements from Settings.dc.html.
// Colours, type sizes and radii come from WildOS.UI's Style.
QtObject {
    readonly property int navWidth: 300
    readonly property int contentLeft: 20
    readonly property int contentRight: 40
    readonly property int sectionGap: 22
}
