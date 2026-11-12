import QtQuick
import WildOS.PDF
import WildOS.UI as UI

// Find in the document: a small bar at the top right of the pages, in the
// menu glass, with how many matches there are and the arrows to step
// through them. Enter goes to the next, Shift+Enter the one before.
Rectangle {
    id: bar

    required property var app

    function start() {
        visible = true;
        field.forceActiveFocus();
        field.selectAll();
    }
    function stop() {
        visible = false;
        field.text = "";
        app.clearFind();
        app.focusPages();
    }

    visible: false
    width: row.implicitWidth + 16
    height: 48
    radius: UI.Style.radiusMd
    color: UI.Style.glassMenu
    border.width: 1
    border.color: UI.Style.borderMenu

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 4

        UI.SearchField {
            id: field
            width: 240
            height: 32
            placeholderText: qsTr("Find in this PDF")
            onTextChanged: findTimer.restart()
            onEscapePressed: bar.stop()
            Keys.onReturnPressed: event => {
                findTimer.stop();
                if (bar.app.findText !== text) {
                    bar.app.find(text);
                } else {
                    bar.app.stepHit((event.modifiers & Qt.ShiftModifier) ? -1 : 1);
                }
            }
        }

        UI.Label {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(72, implicitWidth)
            leftPadding: 6
            rightPadding: 4
            size: 12
            color: UI.Style.inkMuted
            text: field.text.trim().length === 0 ? "" : bar.app.finding ? qsTr("Finding")
                : bar.app.hits.length === 0 ? qsTr("No matches") : qsTr("%1 of %2").arg(bar.app.hitIndex + 1).arg(bar.app.hits.length)
        }

        UI.IconButton {
            anchors.verticalCenter: parent.verticalCenter
            width: 32
            height: 32
            iconName: "chevron-up"
            iconSize: 14
            tip: qsTr("Previous match")
            enabled: bar.app.hits.length > 0
            onClicked: bar.app.stepHit(-1)
        }
        UI.IconButton {
            anchors.verticalCenter: parent.verticalCenter
            width: 32
            height: 32
            iconName: "chevron-down"
            iconSize: 14
            tip: qsTr("Next match")
            enabled: bar.app.hits.length > 0
            onClicked: bar.app.stepHit(1)
        }
        UI.IconButton {
            anchors.verticalCenter: parent.verticalCenter
            width: 32
            height: 32
            iconName: "close"
            iconSize: 13
            tip: qsTr("Close")
            onClicked: bar.stop()
        }
    }

    Timer {
        id: findTimer
        interval: 250
        onTriggered: bar.app.find(field.text)
    }
}
