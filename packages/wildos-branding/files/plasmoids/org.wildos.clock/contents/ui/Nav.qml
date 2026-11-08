pragma Singleton

import QtQuick

// Focus rings only show while someone is using the keyboard, as with
// :focus-visible in the design. Pointer movement over the cards clears it.
QtObject {
    property bool keyboard: false
    // The button the keyboard is on, across both cards.
    property Item current: null
}
