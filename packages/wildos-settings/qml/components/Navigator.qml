pragma Singleton

import QtQuick

// Lets any page open another without reaching up through the item tree.
// The window registers itself here when it starts.
QtObject {
    property var window: null
    // Something a page should do once it is open, asked for from outside
    // (wildos-settings --page signin-wallet asks Sign-in options to open
    // its saved passwords dialog). The page clears it when done.
    property string request: ""

    function open(id) {
        if (window) {
            window.openPage(id);
        }
    }
}
