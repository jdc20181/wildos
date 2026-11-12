import QtQuick

// The design's trust chip: a small rounded label for what something can
// reach, accent tinted when it is granted and plain when held back
// (WinAppsSettings.dc). From wildos-settings Chip. `tone` "success" or
// "warning" makes it a status chip instead (Recommended, Activated,
// Experimental in WinModeHome.dc and WinModeSettings.dc).
Rectangle {
    id: chip

    property string text
    property bool granted: true
    property string iconName
    property string tone

    readonly property color toneInk: tone === "success" ? Style.success : tone === "warning" ? Style.warning : Style.accentText

    implicitWidth: row.implicitWidth + 16
    implicitHeight: Math.max(22, row.implicitHeight + 6)
    radius: height / 2
    color: tone === "success" ? Style.successSoft : tone === "warning" ? Style.warningSoft : granted ? Style.accentChip : Style.tone(0.07)
    border.width: granted && tone.length === 0 ? 1 : 0
    border.color: Style.accentBorder

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 5

        Icon {
            visible: chip.iconName.length > 0
            anchors.verticalCenter: parent.verticalCenter
            name: chip.iconName
            size: 12
            strokeWidth: 2
            color: chip.tone.length > 0 ? chip.toneInk : chip.granted ? Style.accentText : Style.inkMuted
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: chip.text
            size: 11
            font.weight: Style.weightMedium
            color: chip.tone.length > 0 ? chip.toneInk : chip.granted ? Style.accentText : Style.inkStatus
        }
    }
}
