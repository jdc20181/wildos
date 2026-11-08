import QtQuick
import QtQuick.Effects
import WildOS.UI as UI

// Draws an item with rounded corners; Qt Quick only clips to rectangles.
Item {
    id: root

    property Item source
    property real radius: UI.Style.radiusMd

    MultiEffect {
        anchors.fill: parent
        source: root.source
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        visible: false
        layer.enabled: true
    }
}
