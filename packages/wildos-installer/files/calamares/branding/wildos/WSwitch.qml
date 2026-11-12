/* The WildOS.UI switch with the word for its state beside it, so the state
 * does not rely on colour alone (Account.dc, Installer.dc).
 */
import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

RowLayout {
    id: control

    property bool checked: false
    property string accessibleName: ""
    signal toggled(bool checked)

    spacing: 12

    Text {
        text: control.checked ? qsTr("On") : qsTr("Off")
        color: UI.Style.inkMuted
        font.family: UI.Style.fontFamily
        font.letterSpacing: UI.Style.tracking(12)
        font.pixelSize: 12
    }

    UI.Toggle {
        id: toggle
        checked: control.checked
        text: ""
        Accessible.name: control.accessibleName
        onToggled: control.toggled(toggle.checked)
    }
}
