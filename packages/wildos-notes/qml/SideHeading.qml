import QtQuick
import WildOS.UI as UI

// "Notebooks" and "Tags" over their rows: 12px semibold, muted.
UI.Label {
    leftPadding: 14
    rightPadding: 14
    topPadding: 14
    bottomPadding: 6
    size: 12
    font.weight: UI.Style.weightSemiBold
    color: UI.Style.inkSubtle
    Accessible.role: Accessible.Heading
}
