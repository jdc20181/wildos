import QtQuick
import WildOS.UI as UI

// The name editor under an icon (F2). Enter or clicking away keeps the new
// name, Escape puts the old one back. With extensions hidden only the name
// is edited and the extension stays, as on Windows.
Rectangle {
    id: field

    property string original
    property bool active: false
    signal commit(string text)
    signal cancel()

    property bool done: false

    radius: 4
    color: UI.Style.windowOpaque
    border.width: 1
    border.color: UI.Style.accent
    height: Math.max(input.contentHeight, UI.Style.px(12) * 1.3) + 8

    onActiveChanged: {
        if (active) {
            done = false;
            input.text = original;
            input.selectAll();
            input.forceActiveFocus();
        }
    }

    function finish(keep) {
        if (done || !active) {
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
        x: 4
        y: 4
        width: parent.width - 8
        horizontalAlignment: TextInput.AlignHCenter
        wrapMode: TextInput.WrapAtWordBoundaryOrAnywhere
        color: UI.Style.inkField
        selectionColor: UI.Style.accentSelected
        selectedTextColor: UI.Style.ink
        font.family: UI.Style.fontFamily
        font.pixelSize: UI.Style.px(12)
        selectByMouse: true
        Accessible.role: Accessible.EditableText
        Accessible.name: "New name"

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
