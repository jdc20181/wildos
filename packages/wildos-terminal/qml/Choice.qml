import QtQuick
import WildOS.UI as UI

// A drop-down bound to a setting: `options` is [{ value, text }], `value`
// the setting's current value. Choosing emits activated(value); the shown
// choice follows the setting again afterwards, so a change made elsewhere
// (a profile deleted, another window) still shows.
UI.ComboBox {
    id: control

    property var options: []
    property var value

    signal chosen(var value)

    function indexOfOption(v) {
        return options.findIndex(o => o.value === v);
    }

    model: options
    currentIndex: indexOfOption(value)
    onActivated: index => {
        const v = options[index].value;
        currentIndex = Qt.binding(() => indexOfOption(value));
        if (v !== value) {
            chosen(v);
        }
    }
}
