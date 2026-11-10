import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import QtQuick.Window
import WildOS.UI as UI
import WildOS.WinMode.Backend

// Windows mode setup, boards 9.1 to 9.7: a 1040 by 720 sheet with the steps
// at the left. It opens on whatever the daemon is doing: the checks when
// Windows mode is not set up, progress while it sets up, Ready once done.
UI.AppWindow {
    id: window

    property string backdrop
    property string board

    // check, licence, options, setup or ready
    property string page: "check"
    property string licenceKind: "none"
    property var choice: ({ licence: "firmware", key: "", homeChoice: "" })
    property string problem
    property string startError

    readonly property int step: page === "check" || page === "licence" ? 1 : page === "options" ? 2 : page === "setup" ? 3 : 4

    width: board === "problems" ? 1440 : 1040
    height: board === "problems" ? 900 : 720
    minimumWidth: 1040
    minimumHeight: 720
    visible: true
    deep: true
    title: "Windows mode"

    Component.onCompleted: {
        switch (board) {
        case "home": licenceKind = "home"; page = "licence"; break;
        case "nolicence": licenceKind = "none"; page = "licence"; break;
        case "options": page = "options"; break;
        case "setup": page = "setup"; break;
        case "download": page = "setup"; problem = "download"; break;
        case "ready": page = "ready"; break;
        }
    }

    // Follow the daemon: a setup already under way, or done, is shown as such.
    Connections {
        target: Setup
        function onStatusChanged() {
            const s = Setup.status.state;
            if (Setup.preview) {
                return;
            }
            if (s === "setup" && window.page !== "setup") {
                window.page = "setup";
            } else if (s === "ready" && window.page === "setup") {
                window.page = "ready";
            } else if (s === "ready" && window.page === "check") {
                window.page = "ready";
            }
            const kind = (Setup.status.problem || {}).kind || "";
            if (s === "failed" && window.page === "setup") {
                window.problem = "failed";
            } else if ((kind === "download" || kind === "space") && window.page === "setup") {
                window.problem = kind;
            }
        }
    }

    function afterChecks() {
        if (Setup.licence === "firmware" && Setup.edition === "home") {
            licenceKind = "home";
            page = "licence";
        } else if (Setup.licence === "firmware") {
            choice = { licence: "firmware", key: "", homeChoice: "" };
            page = "options";
        } else {
            licenceKind = Setup.licence === "inuse" ? "inuse" : "none";
            page = "licence";
        }
    }

    function startSetup() {
        startError = Setup.startSetup(choice.licence, choice.key, choice.homeChoice);
        if (startError.length === 0) {
            page = "setup";
        } else {
            problem = "failed";
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            Layout.fillHeight: true
            current: window.step
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: ["check", "licence", "options", "setup", "ready"].indexOf(window.page)

            StepCheck {
                onNext: window.afterChecks()
                onLater: window.close()
                onProblem: kind => window.problem = kind
            }

            StepLicence {
                kind: window.licenceKind
                onBack: window.page = "check"
                onSkipped: window.close()
                onChosen: (licence, key, homeChoice) => {
                    window.choice = { licence: licence, key: key, homeChoice: homeChoice };
                    window.page = "options";
                }
            }

            StepOptions {
                onBack: window.page = window.choice.licence === "firmware" && window.licenceKind !== "home" ? "check" : "licence"
                onSetUp: window.startSetup()
            }

            StepSetup {
                onCloseRequested: window.close()
            }

            StepReady {
                onDone: window.close()
            }
        }
    }

    // Closing is always possible until setup starts; from then on the
    // setup and ready steps have their own buttons, since closing no longer
    // stops what is under way.
    UI.IconButton {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 14
        anchors.rightMargin: 14
        visible: window.page === "check" || window.page === "licence" || window.page === "options"
        iconName: "close"
        tip: "Close"
        onClicked: window.close()
    }

    T.Popup {
        id: problemPopup
        parent: T.Overlay.overlay
        anchors.centerIn: parent
        modal: true
        visible: window.problem.length > 0
        padding: 0
        width: 420
        height: contentItem.implicitHeight
        closePolicy: T.Popup.CloseOnEscape
        onClosed: window.problem = ""
        T.Overlay.modal: Rectangle {
            radius: UI.Style.windowRadius
            color: UI.Style.scrim
        }
        background: null
        contentItem: ProblemCard {
            solid: true
            kind: window.problem
            received: (Setup.status.problem || {}).received || 0
            failure: window.startError.length > 0 ? window.startError : ((Setup.status.problem || {}).text || "")
            detail: window.startError.length > 0 ? "" : ((Setup.status.problem || {}).detail || "")
            onDismissed: problemPopup.close()
            onRetry: {
                if (window.startError.length > 0 || Setup.status.state === "failed") {
                    window.startError = "";
                    window.page = "options";
                }
            }
        }
        onOpened: contentItem.takeFocus()
    }

    Loader {
        anchors.fill: parent
        active: window.board === "problems"
        sourceComponent: ProblemsBoard {
            backdrop: window.backdrop
        }
    }
}
