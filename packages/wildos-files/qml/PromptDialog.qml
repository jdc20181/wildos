import QtQuick
import QtQuick.Layouts
import WildOS.Files
import WildOS.UI as UI

// Questions from a copy, move or delete in progress: name clashes, files
// that cannot be read, and confirmations before anything is lost.
UI.Dialog {
    id: prompt

    required property bool mine
    readonly property var p: FileOps.prompt
    readonly property string kind: p.kind ?? ""
    property bool all: false
    property bool answered: false

    title: p.title ?? ""
    text: p.text ?? ""

    // Reads p itself: kind is a binding on p and may not have caught up when
    // this handler runs, which left every question unasked and the queue
    // behind it stuck (Empty Recycle Bin did nothing).
    onPChanged: {
        all = false;
        answered = false;
        if ((p.kind ?? "").length > 0 && mine) {
            open();
        } else {
            close();
        }
    }
    onMineChanged: if (mine && kind.length > 0) open()

    // Escape, or the job ending under it, counts as Cancel.
    onAboutToHide: if (!answered && kind.length > 0) {
        answered = true;
        FileOps.answer("cancel", false);
    }

    function reply(choice) {
        answered = true;
        close();
        FileOps.answer(choice, all);
    }

    GridLayout {
        visible: (prompt.p.incoming ?? "").length > 0
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 16
        rowSpacing: 4
        UI.Label { text: "Copying"; color: UI.Style.inkMuted; font.pixelSize: UI.Style.px(12) }
        UI.Label { text: prompt.p.incoming ?? ""; font.pixelSize: UI.Style.px(12) }
        UI.Label { text: "Already there"; color: UI.Style.inkMuted; font.pixelSize: UI.Style.px(12) }
        UI.Label { text: prompt.p.existing ?? ""; font.pixelSize: UI.Style.px(12) }
    }

    UI.CheckBox {
        visible: prompt.p.multiple === true
        text: "Do this for every clash"
        checked: prompt.all
        onToggled: prompt.all = checked
    }

    // The main choice sits at the right, as in DialogCopy.dc.html.
    buttons: [
        UI.Button {
            visible: prompt.p.informational !== true
            text: prompt.kind === "confirm" ? "Cancel" : "Stop"
            onClicked: prompt.reply("cancel")
        },
        UI.Button {
            visible: (prompt.kind === "conflict" && prompt.p.canSkip === true) || prompt.kind === "skip"
            text: "Skip"
            onClicked: prompt.reply("skip")
        },
        UI.Button {
            visible: prompt.kind === "skip"
            text: "Try again"
            onClicked: prompt.reply("retry")
        },
        UI.Button {
            visible: prompt.kind === "conflict" && prompt.p.canKeepBoth === true
            text: "Keep both"
            onClicked: prompt.reply("both")
        },
        UI.Button {
            visible: prompt.kind === "confirm" && (prompt.p.secondary ?? "").length > 0
            text: prompt.p.secondary ?? ""
            onClicked: prompt.reply("secondary")
        },
        UI.Button {
            visible: prompt.kind === "conflict" && prompt.p.canReplace === true
            kind: "primary"
            text: prompt.p.replace ?? "Replace"
            onClicked: prompt.reply("replace")
        },
        UI.Button {
            visible: prompt.kind === "confirm"
            kind: "primary"
            text: prompt.p.primary ?? "Continue"
            onClicked: prompt.reply("primary")
        }
    ]
}
