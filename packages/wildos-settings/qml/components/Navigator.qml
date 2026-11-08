pragma Singleton

import QtQuick

// Lets any page open another without reaching up through the item tree.
// The window registers itself here when it starts.
QtObject {
    property var window: null

    function open(id) {
        if (window) {
            window.openPage(id);
        }
    }
}
