import QtQuick
import QtQuick.Layouts
import WildOS.Desktop
import WildOS.UI as UI

// Questions from a copy, move or delete in progress: name clashes, files
// that cannot be read, and confirmations before anything is lost.
DeskDialog {
    id: prompt

    readonly property var p: DeskOps.prompt
    readonly property string kind: p.kind ?? ""
    property bool all: false
    property bool answered: false

    title: p.title ?? ""
    defaultAction: () => {
        if (kind === "conflict") {
            reply(p.canReplace === true ? "replace" : p.canKeepBoth === true ? "both" : "skip");
        } else if (kind === "skip") {
            reply("skip");
        } else if (kind === "confirm") {
            reply("primary");
        }
    }
    text: p.text ?? ""

    onPChanged: {
        all = false;
        answered = false;
        if ((p.kind ?? "").length > 0) {
            present();
        } else {
            close();
        }
    }

    // Escape, or the job ending under it, counts as Cancel.
    onAboutToHide: if (!answered && kind.length > 0) {
        answered = true;
        DeskOps.answer("cancel", false);
    }

    function reply(choice) {
        answered = true;
        close();
        DeskOps.answer(choice, all);
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

    buttons: [
        UI.Button {
            visible: prompt.kind === "conflict" && prompt.p.canReplace === true
            kind: "primary"
            text: prompt.p.replace ?? "Replace"
            onClicked: prompt.reply("replace")
        },
        UI.Button {
            visible: prompt.kind === "conflict" && prompt.p.canKeepBoth === true
            text: "Keep both"
            onClicked: prompt.reply("both")
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
            visible: prompt.kind === "confirm"
            kind: "primary"
            text: prompt.p.primary ?? "Continue"
            onClicked: prompt.reply("primary")
        },
        UI.Button {
            visible: prompt.kind === "confirm" && (prompt.p.secondary ?? "").length > 0
            text: prompt.p.secondary ?? ""
            onClicked: prompt.reply("secondary")
        },
        UI.Button {
            visible: prompt.p.informational !== true
            text: prompt.kind === "confirm" ? "Cancel" : "Stop"
            onClicked: prompt.reply("cancel")
        }
    ]
}
