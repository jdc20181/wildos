import QtQuick
import WildOS.Files
import WildOS.UI as UI

// The in-place name editor. Enter or clicking away keeps the new name,
// Escape puts the old one back. With extensions hidden only the name is
// edited and the extension stays, as on Windows.
Rectangle {
    id: field

    required property string original
    property bool multiLine: false
    signal commit(string text)
    signal cancel()

    radius: 4
    color: UI.Style.windowOpaque
    border.width: 1
    border.color: UI.Style.accent
    implicitHeight: multiLine ? Math.min(input.contentHeight + 8, Metrics.fit(52, 12)) : Metrics.fit(26, 13)

    property bool done: false

    function finish(keep) {
        if (done) {
            return;
        }
        done = true;
        if (keep) {
            commit(input.text);
        } else {
            cancel();
        }
    }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        verticalAlignment: TextInput.AlignVCenter
        horizontalAlignment: field.multiLine ? TextInput.AlignHCenter : TextInput.AlignLeft
        wrapMode: field.multiLine ? TextInput.WrapAnywhere : TextInput.NoWrap
        text: field.original
        color: UI.Style.inkField
        selectionColor: UI.Style.accentSelected
        selectedTextColor: UI.Style.ink
        font.family: UI.Style.fontFamily
        font.pixelSize: UI.Style.px(field.multiLine ? 12 : 13)
        clip: true
        selectByMouse: true
        Accessible.role: Accessible.EditableText
        Accessible.name: "New name"

        Component.onCompleted: {
            selectAll();
            forceActiveFocus();
        }
        Keys.onReturnPressed: field.finish(true)
        Keys.onEnterPressed: field.finish(true)
        Keys.onEscapePressed: field.finish(false)
        Keys.onPressed: event => {
            // Characters a name cannot hold are refused as they are typed.
            if (event.text === "/") {
                event.accepted = true;
            }
        }
        onActiveFocusChanged: if (!activeFocus) field.finish(true)
    }
}
