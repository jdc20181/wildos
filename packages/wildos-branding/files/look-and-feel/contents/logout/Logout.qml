import QtQuick
import Qt.labs.folderlistmodel

import org.kde.coreaddons as KCoreAddons
import org.kde.plasma.plasma5support as P5Support
import org.kde.plasma.private.sessions
import WildOS.UI as UI

import "timer.js" as AutoTriggerTimer

// Wild OS power and sign-out screen, shown by ksmserver-logout-greeter on every
// screen. The signals, the context properties it reads (sdtype, maysd,
// canLogout, spdMethods, softwareUpdatePending, rebootToFirmwareSetup,
// rebootToBootLoaderMenu, screenGeometry) and the 30 second countdown follow
// Breeze's Logout.qml; only the look is Wild OS, drawn from the power menu in
// PowerMenu.dc.html at full-screen size.
Item {
    id: root

    width: screenGeometry.width
    height: screenGeometry.height

    signal logoutRequested()
    signal haltRequested()
    signal haltUpdateRequested()
    signal suspendRequested(int spdMethod)
    signal rebootRequested()
    signal rebootRequested2(int opt)
    signal rebootUpdateRequested()
    signal cancelRequested()
    signal lockScreenRequested()
    signal cancelSoftwareUpdateRequested()

    // The greeter reads this to choose between darkening and lightening the
    // blurred desktop behind the window, so it must stay a dark colour.
    property alias backgroundColor: scrim.color

    function sleepRequested(): void {
        root.suspendRequested(2);
    }

    function hibernateRequested(): void {
        root.suspendRequested(4);
    }

    property real timeout: 30
    property real remainingTime: root.timeout

    readonly property bool showAllOptions: sdtype === ShutdownType.ShutdownTypeDefault
    readonly property bool isReboot: sdtype === ShutdownType.ShutdownTypeReboot
    readonly property bool isHalt: sdtype === ShutdownType.ShutdownTypeHalt
    readonly property bool isLogout: !showAllOptions && !isReboot && !isHalt
    readonly property bool updatesPending: typeof softwareUpdatePending !== "undefined" && softwareUpdatePending

    // What happens when the countdown runs out: the action the screen was
    // opened for.
    function runCurrentAction(): void {
        if (root.isReboot) {
            root.updatesPending ? root.rebootUpdateRequested() : root.rebootRequested();
        } else if (root.isHalt) {
            root.updatesPending ? root.haltUpdateRequested() : root.haltRequested();
        } else {
            root.logoutRequested();
        }
    }

    function seconds(n: int): string {
        return n === 1 ? "1 second" : n + " seconds";
    }

    readonly property string countdownText: {
        const n = Math.max(0, Math.ceil(root.remainingTime));
        if (root.isReboot) {
            return (root.updatesPending ? "Installing updates and restarting in " : "Restarting in ") + seconds(n);
        }
        if (root.isHalt) {
            return (root.updatesPending ? "Installing updates and shutting down in " : "Shutting down in ") + seconds(n);
        }
        return "Signing out in " + seconds(n);
    }

    // Keyboard focus starts on the primary choice so Enter confirms it. If
    // that choice is unavailable here, the first one that is shown takes it.
    Component.onCompleted: Qt.callLater(() => {
        const primary = choices.buttons.find(b => b.visible && b.primary);
        const fallback = choices.buttons.find(b => b.visible);
        (primary ?? fallback ?? cancelButton).forceActiveFocus();
    })

    onRemainingTimeChanged: {
        if (remainingTime <= 0) {
            runCurrentAction();
        }
    }

    KCoreAddons.KUser {
        id: kuser
    }

    SessionsModel {
        id: sessionsModel
        includeUnusedSessions: false
    }

    SessionManagement {
        id: sessionManagement
    }

    // The power menu's sign-out screen offers Task Manager too, when it is
    // installed (Ctrl+Alt+Del has its own screen, wildos-system-dialogs). A
    // folder model filtered to the one desktop file is how QML can tell.
    FolderListModel {
        id: taskManagerEntry
        folder: "file:///usr/share/applications"
        nameFilters: ["org.wildos.taskmanager.desktop"]
        showDirs: false
    }

    // kstart starts it as an app, with launch feedback and the taskbar's
    // starting line, rather than as a bare command.
    P5Support.DataSource {
        id: runner
        engine: "executable"
        onNewData: source => disconnectSource(source)
    }

    function openTaskManager(): void {
        runner.connectSource("kstart --application org.wildos.taskmanager");
        root.cancelRequested();
    }

    Timer {
        id: countDownTimer
        running: !root.showAllOptions
        repeat: true
        interval: 1000
        onTriggered: root.remainingTime--
        Component.onCompleted: {
            AutoTriggerTimer.addCancelAutoTriggerCallback(function () {
                countDownTimer.running = false;
            });
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: root.cancelRequested()
    }

    Rectangle {
        id: scrim
        anchors.fill: parent
        // The page ground at partial strength. The greeter reads its
        // lightness to darken or lighten the blurred desktop behind, so in
        // light mode the screen turns pale and the dark ink reads.
        color: Qt.rgba(UI.Style.ground.r, UI.Style.ground.g, UI.Style.ground.b, UI.Style.dark ? 0.30 : 0.45)
        opacity: 0
        Component.onCompleted: opacity = 1

        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.cancelRequested()
    }

    Column {
        id: content

        anchors.centerIn: parent
        anchors.verticalCenterOffset: -24
        spacing: 0
        opacity: 0
        scale: 0.98

        ParallelAnimation {
            running: true
            NumberAnimation { target: content; property: "opacity"; to: 1; duration: 220; easing.type: Easing.OutCubic }
            NumberAnimation { target: content; property: "scale"; to: 1; duration: 260; easing.type: Easing.OutCubic }
        }

        UI.Avatar {
            anchors.horizontalCenter: parent.horizontalCenter
            size: 88
            ring: true
            shadow: true
            source: kuser.faceIconUrl
        }

        Item { width: 1; height: 18 }

        UI.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: kuser.fullName.length > 0 ? kuser.fullName : kuser.loginName
            size: 18
            font.weight: UI.Style.weightSemiBold
        }

        Item { width: 1; height: 8 }

        // Kept in the layout when hidden so nothing jumps as it fades out.
        UI.Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.countdownText
            color: UI.Style.inkMuted
            size: 14
            opacity: countDownTimer.running ? 1 : 0
            visible: !root.showAllOptions

            Behavior on opacity {
                NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
            }
        }

        Item { width: 1; height: 32 }

        Row {
            id: choices

            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12

            readonly property var buttons: [lockButton, switchUserButton, signOutButton, taskManagerButton, sleepButton, hibernateButton, restartButton, shutDownButton]

            function step(from: Item, delta: int): void {
                const shown = buttons.filter(b => b.visible);
                const i = shown.indexOf(from);
                if (i < 0 || shown.length === 0) {
                    return;
                }
                shown[(i + delta + shown.length) % shown.length].forceActiveFocus(delta < 0 ? Qt.BacktabFocusReason : Qt.TabFocusReason);
            }

            PowerButton {
                id: lockButton
                text: "Lock"
                iconName: "lock"
                visible: sessionManagement.canLock
                onClicked: root.lockScreenRequested()
                onNavigate: step => choices.step(lockButton, step)
            }

            PowerButton {
                id: switchUserButton
                text: "Switch user"
                iconName: "switch-user"
                visible: sessionManagement.canSwitchUser
                onClicked: {
                    sessionManagement.switchUser();
                    root.cancelRequested();
                }
                onNavigate: step => choices.step(switchUserButton, step)
            }

            PowerButton {
                id: signOutButton
                text: "Sign out"
                iconName: "sign-out"
                visible: canLogout
                primary: root.isLogout
                focus: root.isLogout
                onClicked: root.logoutRequested()
                onNavigate: step => choices.step(signOutButton, step)
            }

            PowerButton {
                id: taskManagerButton
                text: "Task Manager"
                iconName: "task-manager"
                visible: taskManagerEntry.status === FolderListModel.Ready && taskManagerEntry.count > 0
                onClicked: root.openTaskManager()
                onNavigate: step => choices.step(taskManagerButton, step)
            }

            // Separates leaving the session from the power choices, as the
            // divider does in Start's power menu.
            Item {
                width: 12
                height: 120
                visible: (lockButton.visible || switchUserButton.visible || signOutButton.visible || taskManagerButton.visible)
                         && (sleepButton.visible || hibernateButton.visible || restartButton.visible || shutDownButton.visible)

                Rectangle {
                    anchors.centerIn: parent
                    width: 1
                    height: 64
                    color: UI.Style.borderMenu
                }
            }

            PowerButton {
                id: sleepButton
                text: "Sleep"
                iconName: "moon"
                visible: spdMethods.SuspendState
                onClicked: root.sleepRequested()
                onNavigate: step => choices.step(sleepButton, step)
            }

            PowerButton {
                id: hibernateButton
                text: "Hibernate"
                iconName: "hibernate"
                visible: spdMethods.HibernateState
                onClicked: root.hibernateRequested()
                onNavigate: step => choices.step(hibernateButton, step)
            }

            PowerButton {
                id: restartButton
                text: root.updatesPending ? "Update and restart" : "Restart"
                iconName: "restart"
                visible: maysd
                primary: root.isReboot
                focus: root.isReboot
                onClicked: root.updatesPending ? root.rebootUpdateRequested() : root.rebootRequested()
                onNavigate: step => choices.step(restartButton, step)
            }

            PowerButton {
                id: shutDownButton
                text: root.updatesPending ? "Update and shut down" : "Shut down"
                iconName: "power"
                visible: maysd
                primary: root.isHalt || root.showAllOptions
                focus: root.isHalt || root.showAllOptions
                onClicked: root.updatesPending ? root.haltUpdateRequested() : root.haltRequested()
                onNavigate: step => choices.step(shutDownButton, step)
            }
        }

        Item { width: 1; height: notes.visible ? 20 : 0 }

        Column {
            id: notes

            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.max(choices.width, 420)
            spacing: 6
            visible: otherUsers.visible || firmwareNote.visible || bootMenuNote.visible

            Note {
                id: otherUsers
                visible: sessionsModel.count > 1 && (!root.isLogout || root.showAllOptions)
                text: sessionsModel.count === 2
                      ? "Someone else is signed in. If you shut down or restart, they could lose unsaved work."
                      : (sessionsModel.count - 1) + " other people are signed in. If you shut down or restart, they could lose unsaved work."
            }

            Note {
                id: firmwareNote
                visible: rebootToFirmwareSetup
                text: "Your computer will open its firmware settings when it restarts."
            }

            Note {
                id: bootMenuNote
                visible: rebootToBootLoaderMenu
                text: "Your computer will show the boot menu when it restarts."
            }
        }

        Item { width: 1; height: 28 }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8

            SecondaryButton {
                text: "Restart without updating"
                visible: root.updatesPending && maysd
                onClicked: root.rebootRequested()
            }

            SecondaryButton {
                text: "Shut down without updating"
                visible: root.updatesPending && maysd
                onClicked: root.haltRequested()
            }

            SecondaryButton {
                id: cancelButton
                text: "Cancel"
                onClicked: root.cancelRequested()
            }
        }
    }

    component Note: UI.Label {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: UI.Style.inkMuted
        size: 12
    }

    // Typing on any of these stops the countdown, as on the big choices.
    component SecondaryButton: UI.Button {
        Keys.onPressed: event => {
            AutoTriggerTimer.cancelAutoTrigger();
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                clicked();
                event.accepted = true;
            }
        }
    }
}
