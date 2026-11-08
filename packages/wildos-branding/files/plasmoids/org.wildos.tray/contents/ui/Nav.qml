pragma Singleton

import QtQuick

// Focus rings only show while someone is using the keyboard, as with
// :focus-visible in the design. pointer movement over the popup clears it.
QtObject {
    property bool keyboard: false
}
