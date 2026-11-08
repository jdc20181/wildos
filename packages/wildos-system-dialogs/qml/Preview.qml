import QtQuick
import QtQuick.Effects
import QtQuick.Window
import WildOS.UI as UI
import WildOS.SystemDialogs
import WildOS.SystemDialogs.Backend

// A board drawn the way the artboards are, 1440 by 900 over a picture, for
// design reviews. The compositor's blur is stood in for by blurring the
// picture itself. Boards: permission (and permission-unverified,
// permission-admins, permission-wrong), security, prompts, and Windows
// mode's winmode-shutdown (9.9) and winmode-devices (9.10).
Window {
    id: window

    property string board: "prompts"
    property string backdrop
    property string appIcon: Qt.application.arguments.indexOf("--icon") >= 0 ? Qt.application.arguments[Qt.application.arguments.indexOf("--icon") + 1] : ""

    readonly property bool permission: board.startsWith("permission")
    readonly property bool winmode: board.startsWith("winmode")

    width: 1440
    height: 900
    visible: true
    color: UI.Style.lakeNavy
    title: "Wild OS preview"

    Image {
        id: picture
        anchors.fill: parent
        source: window.backdrop
        fillMode: Image.PreserveAspectCrop
        verticalAlignment: Image.AlignVCenter
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: picture
        blurEnabled: window.board !== "prompts"
        blur: 1.0
        blurMax: window.board === "security" ? 60 : 16
        autoPaddingEnabled: false
    }

    Rectangle {
        anchors.fill: parent
        color: window.permission ? Qt.rgba(UI.Style.scrim.r, UI.Style.scrim.g, UI.Style.scrim.b, 0.55)
             : window.winmode ? UI.Style.scrim
             : window.board === "security" ? Qt.rgba(UI.Style.wallpaperDim.r, UI.Style.wallpaperDim.g, UI.Style.wallpaperDim.b, 0.5)
             : Qt.rgba(UI.Style.scrim.r, UI.Style.scrim.g, UI.Style.scrim.b, 0.2)
    }

    PermissionCard {
        visible: window.permission
        x: 470
        y: 190
        height: implicitHeight
        Component.onCompleted: takeFocus()
    }

    SecurityContent {
        visible: window.board === "security"
        anchors.fill: parent
        previewHover: true
    }

    Item {
        anchors.fill: parent
        visible: window.board === "prompts"

        NotRespondingCard {
            x: 60
            y: 120
            appName: "Invoice Maker"
            iconSource: window.appIcon
        }

        BatteryCard {
            x: 540
            y: 120
            percent: 10
            minutes: 25
            saver: true
        }

        RestartCard {
            x: 1000
            y: 120
            security: true
            times: Prompts.restartTimes
        }

        WindowsModeCard {
            x: 60
            y: 440
            appName: "Invoice Maker"
            iconSource: window.appIcon
        }
    }

    // Board 9.9 and the right of 9.10, with the boards' captions.
    component Caption: Column {
        property string heading
        property string body
        width: 300
        spacing: 4

        UI.Label {
            text: parent.heading
            size: 15
            font.weight: Font.DemiBold
            color: UI.Style.inkOnWallpaper
        }

        UI.Label {
            width: parent.width
            text: parent.body
            size: 13
            color: UI.Style.inkOnWallpaper
            wrapMode: Text.Wrap
            elide: Text.ElideNone
            lineHeight: 1.5
        }
    }

    Item {
        anchors.fill: parent
        visible: window.board === "winmode-shutdown"

        WindowsShutdownCard {
            x: 400
            y: 140
            apps: Prompts.info.apps || []
            reopen: Prompts.info.reopen || ""
        }

        Caption {
            x: 60; y: 160
            heading: "Shut down, restart or update"
            body: "This appears whenever a Windows app has open documents. Choosing nothing for 60 seconds takes the safe option."
        }
        Caption {
            x: 60; y: 330
            heading: "Where files are saved"
            body: "Saving in Word or Excel writes straight into your Wild OS Documents folder, so files are never trapped inside Windows."
        }
        Caption {
            x: 1080; y: 160
            heading: "If power is lost"
            body: "Office's own AutoRecover is pointed at your Documents folder too, so a power cut loses a few minutes at most."
        }
        Caption {
            x: 1080; y: 330
            heading: "Sleep and lid close"
            body: "Windows mode simply sleeps with the PC. No dialog needed."
        }
    }

    Item {
        anchors.fill: parent
        visible: window.board === "winmode-devices"

        Caption {
            x: 800; y: 60
            width: 500
            heading: "When a call app has to use Windows mode"
            body: "Camera, microphone and speakers are passed through to Windows, with the same permission prompt as any other app."
        }

        WindowsDevicesCard {
            x: 800
            y: 150
            appName: Prompts.info.appName || ""
            cameras: Prompts.info.cameras || []
            microphones: Prompts.info.microphones || []
        }
    }

    Component.onCompleted: {
        if (board === "prompts") {
            Prompts.demo("restart");
        } else if (board === "winmode-shutdown") {
            Prompts.demo("windowsshutdown");
        } else if (board === "winmode-devices") {
            Prompts.demo("windowsdevices");
        }
    }
}
