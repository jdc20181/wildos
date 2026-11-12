pragma Singleton

import QtQuick
import WildOS.UI as UI

// The colours a note can be marked with, the small dot in the list. Amber,
// blue and green are the board's (AppNotes.dc, AppNotesLight.dc); the file
// says which by name ("colour: amber"), so it reads without Notes.
QtObject {
    readonly property var all: [
        { id: "amber", name: qsTr("Amber"), dark: "#ffc46a", light: "#ffc46a" },
        { id: "red", name: qsTr("Red"), dark: "#ff8a80", light: "#d9483b" },
        { id: "green", name: qsTr("Green"), dark: "#5fd38d", light: "#5fd38d" },
        { id: "blue", name: qsTr("Blue"), dark: "#8fb4ff", light: "#3563d8" },
        { id: "purple", name: qsTr("Purple"), dark: "#c4b2ff", light: "#7d5fe6" }
    ]

    function colourOf(id: string): color {
        const found = all.find(c => c.id === id);
        return found ? (UI.Style.dark ? found.dark : found.light) : "transparent";
    }
    function known(id: string): bool {
        return all.some(c => c.id === id);
    }
}
