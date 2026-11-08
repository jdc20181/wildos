pragma Singleton

import QtQuick

// Focus rings only show while someone is using the keyboard, as with
// :focus-visible in the design. Any pointer movement clears it.
QtObject {
    property bool keyboard: false
}
