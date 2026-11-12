pragma Singleton

import QtQuick
import WildOS.UI as UI

// The values the board gives Archive that the system tokens do not have,
// as neutral shades of the window glass (AppArchive.dc, AppArchiveLight.dc).
QtObject {
    readonly property bool dark: UI.Style.dark
    readonly property bool contrast: UI.Style.contrast

    // The toolbar (#15171d on #181a20; #f2f2f2 on #f4f4f4), the extract
    // pane and the status bar (#111319; #f0f0f0).
    readonly property color toolbarShade: contrast ? "#000000" : Qt.rgba(0, 0, 0, dark ? 0.12 : 0.008)
    readonly property color paneShade: contrast ? "#000000" : Qt.rgba(0, 0, 0, dark ? 0.12 : 0.06)
    readonly property color statusShade: contrast ? "#000000" : Qt.rgba(0, 0, 0, dark ? 0.28 : 0.016)
    readonly property color hairline: contrast ? UI.Style.contrastDivider : dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.062)

    readonly property color rowHover: contrast ? UI.Style.surfaceHover : dark ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.049)
    readonly property color rowSelected: contrast ? UI.Style.surfaceSelected : dark ? Qt.rgba(1, 1, 1, 0.061) : Qt.rgba(0, 0, 0, 0.043)
    readonly property color rowPressed: contrast ? UI.Style.surfacePressed : dark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.07)

    // The read only note is the one tinted surface: the accent at a tenth.
    readonly property color bannerFill: UI.Style.accentAlpha(0.10, 0.08)
    readonly property color bannerEdge: UI.Style.accentAlpha(0.22, 0.176)
    readonly property color bannerIcon: dark ? UI.Style.accent : UI.Style.accentText

    readonly property color fieldFill: contrast ? "#000000" : dark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.72)
    readonly property color fieldEdge: contrast ? UI.Style.contrastEdge : dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.078)
    readonly property color checkEdge: contrast ? "#ffffff" : dark ? Qt.rgba(1, 1, 1, 0.55) : Qt.rgba(0, 0, 0, 0.429)

    readonly property color folderInk: contrast ? "#ffffff" : dark ? "#f2b84d" : "#96722f"
    readonly property color iconInk: contrast ? "#ffffff" : dark ? Qt.rgba(1, 1, 1, 0.86) : "#2a3147"
    readonly property color fileInk: UI.Style.inkMuted
    readonly property color thumbBlank: UI.Style.surfaceTile

    readonly property string icon: "qrc:/qt/qml/WildOS/Archive/data/wildos-archive.svg"
}
