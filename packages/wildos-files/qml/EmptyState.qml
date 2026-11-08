import QtQuick
import WildOS.Files
import WildOS.UI as UI

// What the content area says when it has nothing to show.
Item {
    id: empty

    required property var folder
    readonly property string message: {
        if (!folder) {
            return "";
        }
        if (folder.errorText.length > 0) {
            return folder.errorText;
        }
        if (folder.count > 0 || (folder.loading && folder.searchText.length === 0)) {
            return "";
        }
        if (folder.searchText.length > 0) {
            return folder.searching ? "Searching…" : "No items match your search.";
        }
        if (folder.kind === "bin") {
            return "The Recycle Bin is empty.";
        }
        return "This folder is empty.";
    }

    visible: message.length > 0

    UI.Label {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 36
        width: Math.min(parent.width - 48, 420)
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: empty.message
        color: UI.Style.inkMuted
    }
}
