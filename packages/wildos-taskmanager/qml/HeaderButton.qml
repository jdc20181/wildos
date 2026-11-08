import QtQuick
import WildOS.UI as UI

// The page header's buttons (TaskManager.dc "Run new task", "End task"):
// the small Wild OS button with a 16px icon and 12px padding.
UI.Button {
    size: "small"
    leftPadding: 12
    rightPadding: 12
    iconSize: 16
    iconStroke: primary ? 2.2 : 1.8
}
