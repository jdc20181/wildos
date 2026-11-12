import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import WildOS.Remote
import WildOS.UI as UI

// A Remote Desktop connection on its way in: the certificate questions
// (RemoteCheck.dc and its changed twin), the sign-in (RemoteSignIn.dc),
// then its own window, one per monitor with all monitors.
Item {
    id: connector

    property var current: null
    // The desktop windows open now, and the questions, for the screen test.
    property var windows: []
    readonly property alias firstDialog: firstDialog
    readonly property alias changedDialog: changedDialog
    readonly property alias signInDialog: signIn

    signal notice(string text)

    anchors.fill: parent

    function start(h, display) {
        Desktops.start(h, display || "");
    }

    Connections {
        target: Desktops
        function onStarted(d) {
            connector.attach(d);
        }
    }

    // Windows mode's desktop starts without the home window; a question
    // brings it up.
    function surface() {
        if (App.desktopOnly && Window.window && !Window.window.visible) {
            Window.window.show();
        }
    }

    function attach(d) {
        current = d;
        d.certificateNeeded.connect((info, changed) => {
            connector.surface();
            if (changed) {
                changedDialog.desktop = d;
                changedDialog.askCertificate({ name: d.name }, { fingerprint: info.fingerprint,
                    previous: [{ fingerprint: info.previousFingerprint || "", seen: "" }] });
            } else {
                firstDialog.desktop = d;
                firstDialog.askCertificate({ name: d.name }, info);
            }
        });
        d.credentialsNeeded.connect((user, reason) => {
            connector.surface();
            signIn.ask(d, user, reason);
        });
        d.retryWanted.connect(user => {
            connector.surface();
            signIn.ask(d, user, "wrong");
        });
        d.opened.connect(() => {
            if (current === d) {
                current = null;
            }
            connector.openWindows(d);
        });
        d.stateChanged.connect(() => {
            if (d.state === "ended" && !d.everConnected) {
                if (current === d) {
                    current = null;
                }
                if (d.endTitle.length > 0) {
                    connector.surface();
                    problem.show(d.endTitle, d.endText);
                }
            }
        });
    }

    // One window, or one per screen, each showing its part of the desktop.
    function openWindows(d) {
        const screens = Qt.application.screens;
        if (d.multiMonitor) {
            // Each screen shows the part of the desktop the engine gave it.
            const layout = d.monitors;
            for (let i = 0; i < layout.length; ++i) {
                const s = screens.find(x => x.name === layout[i].screen);
                if (!s) {
                    continue;
                }
                windows.push(windowComponent.createObject(null, { desktop: d, screen: s, targetScreen: s.name, sourceRect: layout[i].rect, primary: i === 0 }));
            }
        } else {
            windows.push(windowComponent.createObject(null, { desktop: d, primary: true }));
        }
    }

    Component {
        id: windowComponent
        DesktopWindow {}
    }

    // Connecting, before any question or window.
    Rectangle {
        visible: connector.current !== null && connector.current.state === "connecting"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: connectingRow.implicitWidth + 32
        height: 52
        radius: UI.Style.radiusMd
        color: UI.Style.glassMenu
        border.width: 1
        border.color: UI.Style.borderMenu
        z: 50
        RowLayout {
            id: connectingRow
            anchors.centerIn: parent
            spacing: 12
            UI.Spinner {
                size: 18
                running: parent.visible
            }
            UI.Label {
                text: connector.current ? qsTr("Connecting to %1…").arg(connector.current.name) : ""
                size: 14
            }
            UI.Button {
                size: "small"
                text: qsTr("Cancel")
                onClicked: connector.current.disconnectNow()
            }
        }
    }

    HostKeyDialog {
        id: firstDialog
        property var desktop
        onAccepted: (h, result) => desktop.answerCertificate(true, result.remember)
        onDeclined: desktop.answerCertificate(false, false)
    }
    HostChangedDialog {
        id: changedDialog
        property var desktop
        onRebuilt: desktop.answerCertificate(true, true)
        onRefused: desktop.answerCertificate(false, false)
    }
    SignInDialog {
        id: signIn
    }
    UI.Dialog {
        id: problem
        function show(heading, body) {
            title = heading;
            text = body;
            open();
        }
        buttons: [
            UI.Button {
                kind: "primary"
                text: qsTr("OK")
                onClicked: problem.close()
            }
        ]
    }
}
