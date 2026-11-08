import QtQuick
import WildOS.UI as UI

// A short box for a whole number, with its unit after it.
Row {
    id: field

    property int value
    property int from: 0
    property int to: 999
    property string unit
    property string accessibleName

    signal committed(int value)

    spacing: 8

    Field {
        anchors.verticalCenter: parent.verticalCenter
        width: 72
        implicitWidth: 72
        value: String(field.value)
        placeholderText: field.accessibleName
        validator: IntValidator {
            bottom: field.from
            top: field.to
        }
        onCommitted: text => {
            const n = parseInt(text);
            if (!isNaN(n)) {
                field.committed(Math.max(field.from, Math.min(field.to, n)));
            }
        }
    }

    UI.Label {
        anchors.verticalCenter: parent.verticalCenter
        visible: field.unit.length > 0
        text: field.unit
        color: UI.Style.inkMuted
    }
}
