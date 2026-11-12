import QtQuick
import WildOS.UI as UI

// The round badge at the start of the status card: 56px, the status
// colour at a sixth behind its icon.
Rectangle {
    id: circle

    property string iconName: "check"
    property color tone: BackupStyle.good
    property color fill: BackupStyle.goodSoft
    property bool busy: false

    implicitWidth: 56
    implicitHeight: 56
    radius: 28
    color: fill

    BackupIcon {
        anchors.centerIn: parent
        visible: !circle.busy
        name: circle.iconName
        size: 28
        strokeWidth: circle.iconName === "check" ? 2.6 : 2
        color: circle.tone
    }
    UI.Spinner {
        anchors.centerIn: parent
        visible: circle.busy
        size: 26
        color: circle.tone
    }
}
