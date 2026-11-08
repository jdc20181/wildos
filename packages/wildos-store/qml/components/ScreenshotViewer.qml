import QtQuick
import QtQuick.Controls
import WildOS.UI as UI

// Screenshots at full size over the window, with arrows between them.
Popup {
    id: viewer

    property var shots: []
    property int index: 0

    function show(list, at) {
        shots = list;
        index = at;
        open();
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: parent ? parent.width - 80 : 800
    height: parent ? parent.height - 80 : 600
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle {
        color: Metrics.viewerShade
    }

    background: Item {}

    contentItem: Item {
        focus: true
        Keys.onLeftPressed: viewer.index = Math.max(0, viewer.index - 1)
        Keys.onRightPressed: viewer.index = Math.min(viewer.shots.length - 1, viewer.index + 1)

        Image {
            id: big
            anchors.fill: parent
            anchors.margins: 56
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            source: viewer.shots.length > 0 ? viewer.shots[viewer.index].full : ""
        }

        Image {
            anchors.fill: big
            fillMode: Image.PreserveAspectFit
            visible: big.status !== Image.Ready
            source: viewer.shots.length > 0 ? viewer.shots[viewer.index].thumb : ""
        }

        UI.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            text: viewer.shots.length > 0 ? viewer.shots[viewer.index].caption : ""
            color: Metrics.inkSub
        }

        component ViewerButton: UI.IconButton {
            width: 40
            height: 40
            radius: 20
            iconSize: 17
            background: Rectangle {
                radius: 20
                color: parent.down ? UI.Style.surfacePressed : parent.hovered ? UI.Style.surfaceHover : UI.Style.glassMenu
                border.width: 1
                border.color: UI.Style.borderMenu
            }
        }

        ViewerButton {
            visible: viewer.index > 0
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            iconName: "chevron-left"
            tip: "Previous screenshot"
            onClicked: viewer.index--
        }

        ViewerButton {
            visible: viewer.index < viewer.shots.length - 1
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            iconName: "chevron"
            tip: "Next screenshot"
            onClicked: viewer.index++
        }

        ViewerButton {
            anchors.right: parent.right
            anchors.top: parent.top
            iconName: "close"
            tip: "Close"
            onClicked: viewer.close()
        }
    }
}
