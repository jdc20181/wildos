import QtQuick
import WildOS.UI as UI

// Rows stacked in one Wild OS card, as the design groups settings and
// facts; the card masks them to its rounded corners.
UI.Card {
    id: group

    default property alias rows: column.data

    Column {
        id: column
        width: group.availableWidth
    }
}
