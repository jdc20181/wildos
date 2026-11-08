pragma Singleton

import QtQuick

// The welcome sheet's own measurements from the four Welcome artboards
// (Welcome, BringFiles, WelcomePrivacy, StarterApps). Colours, text sizes
// and the accent come from WildOS.UI, which follows the accent picked in
// step 2 as soon as it is written.
QtObject {
    readonly property int radiusSheet: 20
    readonly property int sheetWidth: 1040
    readonly property int sheetHeight: 720
    readonly property int sidebarWidth: 260
}
