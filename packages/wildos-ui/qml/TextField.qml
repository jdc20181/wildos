import QtQuick
import QtQuick.Templates as T

// A single line of input: the sunken field from the artboards (surface
// sunken, 1px 0.10 edge, radius 8, 36px, 13px text) whose edge turns
// accent while typing, as wildos-settings SearchField does. `size: "large"`
// is the 42px, radius 10, 14px field of the account and installer forms
// (Account.dc). A password field gets a show and hide button with
// `revealable`. A right click, the Menu key or Shift+F10 opens the text
// menu (MenusApps.dc "Text you're typing"): Undo, Cut, Copy, Paste, Select
// all and Emoji, with their shortcuts. A single line field holds plain
// text only, so Paste already pastes plain text and the board's "Paste as
// plain text" has nothing to add here; spelling suggestions need a spell
// checked text box, which this is not.
T.TextField {
    id: control

    property string size: "medium"
    property bool revealable: false
    property bool revealed: false
    property bool error: false
    property bool escapeClears: true
    property bool previewFocus: false
    property alias trailing: trailingRow.data
    // Off for a field that brings its own right-click menu.
    property bool textMenu: true

    readonly property bool large: size === "large"
    readonly property bool editingNow: activeFocus || previewFocus

    implicitWidth: 240
    implicitHeight: Math.max(large ? Style.fieldHeightLarge : Style.controlHeight, contentHeight + 16)
    leftPadding: large ? 14 : 12
    rightPadding: (large ? 14 : 12) + (trailingRow.width > 0 ? trailingRow.width + 6 : 0)
    verticalAlignment: TextInput.AlignVCenter
    color: Style.inkField
    selectionColor: Style.accentSelected
    selectedTextColor: Style.ink
    placeholderTextColor: Style.inkPlaceholder
    font.family: Style.fontFamily
    font.pixelSize: Style.px(large ? 14 : 13)
    echoMode: revealable && !revealed ? TextInput.Password : TextInput.Normal
    passwordCharacter: "•"
    opacity: enabled ? 1 : 0.5
    Accessible.name: placeholderText

    property Menu textMenuPopup: null

    function openTextMenu(px: real, py: real): void {
        if (!textMenuPopup) {
            textMenuPopup = menuComponent.createObject(control);
        }
        const secret = echoMode === TextInput.Password || echoMode === TextInput.PasswordEchoOnEdit;
        const entries = [];
        if (!readOnly) {
            entries.push({ text: "Undo", icon: "undo", shortcut: "Ctrl+Z", enabled: canUndo, run: () => control.undo() },
                { separator: true });
        }
        if (!readOnly && !secret) {
            entries.push({ text: "Cut", icon: "menu-cut", shortcut: "Ctrl+X", enabled: selectedText.length > 0, run: () => control.cut() });
        }
        if (!secret) {
            entries.push({ text: "Copy", icon: "menu-copy", shortcut: "Ctrl+C", enabled: selectedText.length > 0, run: () => control.copy() });
        }
        if (!readOnly) {
            entries.push({ text: "Paste", icon: "menu-paste", shortcut: "Ctrl+V", enabled: canPaste, run: () => control.paste() });
        }
        if (entries.length > 0 && !entries[entries.length - 1].separator) {
            entries.push({ separator: true });
        }
        entries.push({ text: "Select all", icon: "select-all", shortcut: "Ctrl+A", enabled: length > 0, run: () => control.selectAll() });
        if (!readOnly && !secret && System.canRun("wildos-clipboard")) {
            entries.push({ text: "Emoji", icon: "emoji", shortcut: "Win+.", run: () => System.run("wildos-clipboard", ["--emoji"]) });
        }
        forceActiveFocus(Qt.PopupFocusReason);
        textMenuPopup.entries = entries;
        textMenuPopup.popup(control, px, py);
    }

    Component {
        id: menuComponent
        Menu {
            minimumWidth: 236
        }
    }

    TapHandler {
        enabled: control.textMenu && control.enabled
        acceptedButtons: Qt.RightButton
        onTapped: eventPoint => control.openTextMenu(eventPoint.position.x, eventPoint.position.y)
    }

    Keys.onPressed: event => {
        if (textMenu && (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier)))) {
            const caret = cursorRectangle;
            openTextMenu(caret.x, caret.y + caret.height);
            event.accepted = true;
        }
    }

    Keys.onEscapePressed: event => {
        // A type that handles Escape itself (SearchField) turns this off;
        // both handlers run, so this one must then leave the event alone.
        if (!escapeClears) {
            return;
        }
        if (text.length > 0 && echoMode !== TextInput.Password) {
            clear();
            event.accepted = true;
        } else {
            event.accepted = false;
        }
    }

    background: Rectangle {
        radius: control.large ? 10 : Style.radiusSm
        color: Style.surfaceSunken
        border.width: 1
        border.color: control.error ? Style.danger : control.editingNow ? Style.accentBorderStrong : control.large ? Style.borderControl : Style.borderField
        Behavior on border.color {
            ColorAnimation { duration: Style.hoverDuration }
        }
    }

    Label {
        x: control.leftPadding
        width: control.width - control.leftPadding - control.rightPadding
        height: control.height
        visible: control.text.length === 0 && control.placeholderText.length > 0 && control.preeditText.length === 0
        text: control.placeholderText
        size: control.large ? 14 : 13
        color: Style.inkPlaceholder
    }

    Row {
        id: trailingRow
        anchors.right: parent.right
        anchors.rightMargin: control.large ? 8 : 6
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        IconButton {
            visible: control.revealable
            width: 28
            height: 28
            radius: Style.radiusXs
            iconName: control.revealed ? "eye-off" : "eye"
            iconSize: 15
            tip: control.revealed ? "Hide password" : "Show password"
            focusPolicy: Qt.NoFocus
            onClicked: control.revealed = !control.revealed
        }
    }
}
