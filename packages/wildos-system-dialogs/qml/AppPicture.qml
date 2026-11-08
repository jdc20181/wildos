import QtQuick
import WildOS.UI as UI

// The asking app's own icon at 40px, or a plain program icon when it has
// none. `iconSource` takes a file path instead of a theme name.
UI.ThemeIcon {
    property string iconName
    property string iconSource

    width: 40
    height: 40
    implicitWidth: 40
    implicitHeight: 40
    name: iconSource.length > 0 ? iconSource : iconName.length > 0 ? iconName : "application-x-executable"
    fallback: "application-x-executable"
}
