import QtQuick
import QtQuick.Window
import WildOS.UI as UI
import WildOS.SystemDialogs
import WildOS.SystemDialogs.Backend

// The window that holds whichever system prompt is showing. Battery,
// restart and Windows mode float centred above every window (a layer
// surface, so an app cannot cover them); "not responding" is attached to
// the frozen app's window, as Windows does it.
Window {
    id: window

    readonly property int margin: 64

    visible: false
    color: "transparent"
    width: (loader.item ? loader.item.width : 440) + margin * 2
    height: (loader.item ? loader.item.implicitHeight : 200) + margin * 2
    title: loader.item ? loader.item.title : "Wild OS"

    Surface {
        id: surface
        window: window
        role: Prompts.showing === "notresponding" ? "dialog" : "prompt"
        scope: "wildos-prompt"
        blurRect: loader.item ? Qt.rect(window.margin, window.margin, loader.item.width, loader.item.implicitHeight) : Qt.rect(0, 0, 0, 0)
        blurRadius: UI.Style.radiusLg
        parentHandle: Prompts.info.handle || ""
    }

    Loader {
        id: loader
        x: window.margin
        y: window.margin
        focus: true
        sourceComponent: {
            switch (Prompts.showing) {
            case "battery": return battery;
            case "restart": return restart;
            case "notresponding": return notResponding;
            case "windowsmode": return windowsMode;
            case "windowsshutdown": return windowsShutdown;
            case "windowsdevices": return windowsDevices;
            case "windowsstarting": return windowsStarting;
            default: return null;
            }
        }
        onLoaded: {
            surface.show();
            item.takeFocus();
        }
    }

    Connections {
        target: Prompts
        function onShowingChanged() {
            if (Prompts.showing.length === 0) {
                surface.hide();
            }
        }
    }

    Component {
        id: battery
        BatteryCard {
            percent: Prompts.info.percent || 0
            minutes: Prompts.info.minutes || 0
            saver: Prompts.info.saver === true
            saverOn: Prompts.info.saverOn === true
            onDismissed: Prompts.dismiss()
            onSaverChosen: Prompts.batterySaver()
        }
    }

    Component {
        id: restart
        RestartCard {
            security: Prompts.info.security === true
            summary: Prompts.info.summary || ""
            countdown: Prompts.info.mode === "countdown"
            seconds: Prompts.countdown
            scheduled: Prompts.info.scheduled || ""
            times: Prompts.restartTimes
            onRestartNow: Prompts.restartNow()
            onScheduleAt: when => Prompts.schedule(when)
            onNotNow: Prompts.notNow()
        }
    }

    Component {
        id: notResponding
        NotRespondingCard {
            appName: Prompts.info.appName || ""
            iconName: Prompts.info.iconName || ""
            canEnd: Prompts.info.local === true
            onEndTask: Prompts.endTask()
            onWait: Prompts.dismiss()
        }
    }

    Component {
        id: windowsShutdown
        WindowsShutdownCard {
            action: Prompts.info.action || "shutdown"
            apps: Prompts.info.apps || []
            reopen: Prompts.info.reopen || ""
            seconds: Prompts.countdown
            onKeep: Prompts.answerWindowsShutdown("keep")
            onDiscard: Prompts.answerWindowsShutdown("discard")
            onBack: Prompts.answerWindowsShutdown("back")
        }
    }

    Component {
        id: windowsDevices
        WindowsDevicesCard {
            appName: Prompts.info.appName || ""
            cameras: Prompts.info.cameras || []
            microphones: Prompts.info.microphones || []
            onAllowed: (camera, microphone) => Prompts.answerWindowsDevices(true, camera, microphone)
            onDenied: Prompts.answerWindowsDevices(false, "", "")
        }
    }

    Component {
        id: windowsStarting
        WindowsStartingCard {
            appName: Prompts.info.appName || ""
            iconName: Prompts.info.iconName || ""
            seconds: Prompts.info.seconds || 60
            cold: Prompts.info.cold === true
            onHidden: Prompts.dismiss()
        }
    }

    Component {
        id: windowsMode
        WindowsModeCard {
            appName: Prompts.info.appName || ""
            iconName: Prompts.info.iconName || ""
            onCancelled: Prompts.dismiss()
            onAccepted: Prompts.useWindowsMode()
        }
    }
}
