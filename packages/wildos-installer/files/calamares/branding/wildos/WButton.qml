/* The installer's buttons are WildOS.UI buttons in the large size (40px,
 * radius 10, 14px), the size of every Wild OS footer. This keeps the
 * installer pages' short form: primary for the accent button, quiet for a
 * text button.
 */
import QtQuick
import WildOS.UI as UI

UI.Button {
    property bool primary: false
    property bool quiet: false

    kind: primary ? "primary" : (quiet ? "text" : "secondary")
    size: "large"
}
