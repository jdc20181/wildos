import QtQuick
import QtQuick.Effects
import WildOS.Welcome
import WildOS.UI as UI

// The little desktop inside each taskbar choice: the person's wallpaper
// with a sketch of the bar, centred like Windows 11 or along the bottom
// like Windows 10.
Item {
    id: preview

    property bool leftAligned: false
    // Stand-ins for app icons (Start, Files, Browser, Store, Terminal);
    // app icons look the same in either scheme, so these are drawing, not
    // interface colours.
    readonly property var dots: [
        { colour: "#ffffff", round: true },
        { colour: "#f2b84d", round: false },
        { colour: "#3a83ea", round: true },
        { colour: "#eef2fa", round: false },
        { colour: "#2fae6b", round: false }
    ]

    Item {
        id: picture
        anchors.fill: parent
        visible: false
        layer.enabled: true

        Image {
            readonly property real fit: implicitWidth > 0 ? Math.max(parent.width / implicitWidth, parent.height / implicitHeight) : 1
            width: implicitWidth * fit
            height: implicitHeight * fit
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            source: Setup.wallpaper
            sourceSize.width: 720
            asynchronous: true
        }
    }

    Rectangle {
        id: corners
        anchors.fill: parent
        radius: UI.Style.radiusSm
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: parent
        source: picture
        maskEnabled: true
        maskSource: corners
    }

    Rectangle {
        visible: !preview.leftAligned
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        width: centredDots.implicitWidth + 12
        height: 16
        radius: 6
        color: UI.Style.glassBar

        Row {
            id: centredDots
            anchors.centerIn: parent
            spacing: 4

            Repeater {
                model: preview.dots

                Rectangle {
                    required property var modelData
                    width: 8
                    height: 8
                    radius: modelData.round ? 4 : 2
                    color: modelData.colour
                }
            }
        }
    }

    Rectangle {
        visible: !preview.leftAligned
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        width: 34
        height: 16
        radius: 6
        color: UI.Style.glassBar
    }

    // A square-cornered strip; the picture's rounding clips its ends.
    Item {
        visible: preview.leftAligned
        anchors.fill: parent
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: corners
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 18
            color: UI.Style.glassBar

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Repeater {
                    model: preview.dots

                    Rectangle {
                        required property var modelData
                        width: 8
                        height: 8
                        radius: modelData.round ? 4 : 2
                        color: modelData.colour
                    }
                }
            }

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 8
                radius: 2
                color: UI.Style.tone(0.4)
            }
        }
    }
}
