import QtQuick
import WildOS.Office
import WildOS.UI as UI

// Write's or Tables' loading splash (board 12.5) while LibreOffice starts a
// document cold, in place of LibreOffice's own.
UI.LoadingSplash {
    // What is opening, or empty for a new document.
    property string fileName: ""
    // Launching the app rather than opening a document: shown at once, and
    // the start screen follows instead of the program ending.
    property bool launching: false

    appName: Office.appName
    iconSource: Qt.resolvedUrl(Office.tables ? "data/org.wildos.tables.svg" : "data/org.wildos.write.svg")
    accent: Office.tables ? "#2fd48a" : "#9d7bff"
    accentDeep: Office.tables ? "#0d7a49" : "#5631c9"
    status: launching ? qsTr("Starting up")
        : fileName.length > 0 ? qsTr("Opening %1").arg(fileName)
        : (Office.tables ? qsTr("Opening a new workbook") : qsTr("Opening a new document"))
    noteStart: Office.tables ? qsTr("Opens and saves Excel files") : qsTr("Opens and saves Word files")
    noteEnd: qsTr("Built on LibreOffice")
    finishOnFocusLoss: !launching
    showDelay: launching ? 0 : 333
    minimumTime: launching ? 1200 : 0
    onFinished: {
        if (!launching) {
            Qt.quit();
        }
    }

    // LibreOffice can take half a minute on a slow PC; past that the splash
    // goes anyway rather than staying over the desktop.
    Timer {
        interval: 60000
        running: true
        onTriggered: finish()
    }
}
