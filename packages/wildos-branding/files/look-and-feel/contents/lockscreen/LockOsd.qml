import QtQuick

import org.kde.kirigami as Kirigami
import WildOS.UI as UI

import "components"

// Volume and brightness feedback while locked. kscreenlocker finds this item
// by its objectName, sets icon, osdValue, osdMaxValue and showingProgress,
// then calls show().
Item {
    id: osd

    property int timeout: 1800
    property var osdValue: 0
    property real osdMaxValue: 100
    property string icon: ""
    property bool showingProgress: false

    objectName: "onScreenDisplay"
    visible: false
    width: Math.max(220, content.implicitWidth + 32)
    height: 52

    function show() {
        osd.visible = true;
        hideAnimation.restart();
    }

    Glass {
        anchors.fill: parent
        tint: UI.Style.glassMenu
        borderColor: UI.Style.borderMenu
        radius: height / 2
    }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 12

        Kirigami.Icon {
            anchors.verticalCenter: parent.verticalCenter
            width: 20
            height: 20
            source: osd.icon
            color: UI.Style.iconInk
            isMask: true
        }

        Item {
            visible: osd.showingProgress
            anchors.verticalCenter: parent.verticalCenter
            width: 130
            height: 4

            Rectangle {
                anchors.fill: parent
                radius: 2
                color: UI.Style.surfaceTrack
            }
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, osd.osdMaxValue > 0 ? osd.osdValue / osd.osdMaxValue : 0))
                height: parent.height
                radius: 2
                color: UI.Style.accent
            }
        }

        UI.Label {
            anchors.verticalCenter: parent.verticalCenter
            // Without a progress bar the value is a text message, such as a
            // keyboard layout name.
            text: osd.showingProgress ? Math.round(osd.osdMaxValue > 0 ? osd.osdValue / osd.osdMaxValue * 100 : 0) + "%"
                : (typeof osd.osdValue === "string" ? osd.osdValue : "")
            visible: text.length > 0
        }
    }

    SequentialAnimation {
        id: hideAnimation
        PauseAnimation { duration: 200 }
        NumberAnimation {
            target: osd
            property: "opacity"
            from: 1
            to: 0
            duration: osd.timeout
            easing.type: Easing.InQuad
        }
        ScriptAction {
            script: {
                osd.visible = false;
                osd.opacity = 1;
                osd.icon = "";
                osd.osdValue = 0;
            }
        }
    }
}
