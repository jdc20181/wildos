pragma Singleton

import QtQuick
import WildOS.UI as UI

// The Store's layout from Store.dc.html and StoreApp.dc.html, and the few
// colours only its pages use. Every colour comes from WildOS.UI tokens, so
// the accent, the text size and light mode reach the Store with the rest of
// the system.
QtObject {
    readonly property int titleBarHeight: 52
    readonly property int navWidth: 200
    readonly property int contentLeft: 12
    readonly property int contentRight: 32
    readonly property int contentTop: 6
    readonly property int sectionGap: 26
    readonly property int radiusHero: 16

    // Body copy and card captions sit between ink and muted ink.
    readonly property color inkBody: UI.Style.inkAlpha(0.85, 0.80)
    readonly property color inkSub: UI.Style.inkAlpha(0.82, 0.76)
    readonly property color inkCard: UI.Style.inkAlpha(0.75, 0.70)
    readonly property color chip: UI.Style.tone(0.07)
    readonly property color iconTile: UI.Style.surfaceControl
    readonly property color tileHover: UI.Style.tone(0.075)
    readonly property color warningBorder: Qt.rgba(UI.Style.warning.r, UI.Style.warning.g, UI.Style.warning.b, 0.30)

    // The hero banner is a photograph in either scheme: white words and a
    // navy shade over it, and an eyebrow in the accent lifted most of the
    // way to white (Glacier gives #dbe6ff).
    readonly property color onPhoto: UI.Style.inkOnWallpaper
    function onPhotoAlpha(alpha) {
        return Qt.rgba(onPhoto.r, onPhoto.g, onPhoto.b, alpha);
    }
    readonly property color heroShade: Qt.rgba(Qt.darker(UI.Style.lakeNavy, 2.4).r, Qt.darker(UI.Style.lakeNavy, 2.4).g, Qt.darker(UI.Style.lakeNavy, 2.4).b, 0.48)
    readonly property color eyebrow: Qt.rgba(UI.Style.systemAccent.r + (1 - UI.Style.systemAccent.r) * 0.7, UI.Style.systemAccent.g + (1 - UI.Style.systemAccent.g) * 0.7, UI.Style.systemAccent.b + (1 - UI.Style.systemAccent.b) * 0.7, 1)
    // Full-size screenshots sit on a deep shade, darker than a dialog's.
    readonly property color viewerShade: Qt.rgba(UI.Style.scrim.r, UI.Style.scrim.g, UI.Style.scrim.b, 0.8)
}
