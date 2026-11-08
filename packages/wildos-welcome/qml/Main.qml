import QtQuick
import QtQuick.Effects
import QtQuick.Window
import WildOS.Welcome
import WildOS.UI as UI

// The first run: a glass sheet over the wallpaper, dimmed, as drawn in the
// First run artboards (3.3 to 3.9). It covers the whole screen so nothing
// else competes for attention until setup is done.
//
// With setupMode it is the first boot setup session (wildos-setup), all
// seven steps before any account exists, with no way out but finishing:
// the account is made at step 3, and steps 4 to 7 are saved for it and
// applied when Finish signs in to it. Without, it is the same four steps
// for an account made later, which apply as they are chosen.
Window {
    id: root

    property bool windowed: false
    property bool setupMode: false
    property int startStep: 1
    property int step: Math.min(startStep, stepNames.length)

    readonly property var stepNames: (setupMode ? ["Region and language", "Wi-Fi", "Your account"] : [])
        .concat(["Make it feel like home", "Bring your files across", "Your privacy", "Your Windows programs"])

    width: 1440
    height: 900
    visible: true
    visibility: windowed ? Window.Windowed : Window.FullScreen
    flags: windowed ? Qt.Window : (Qt.Window | Qt.FramelessWindowHint)
    color: UI.Style.lakeNavy
    title: "Welcome to Wild OS"

    // Nothing may close the setup session but finishing it: the session
    // ends with this window, and SDDM would only start it again.
    onClosing: close => {
        if (setupMode) {
            close.accepted = false;
        }
    }

    // Wi-Fi has nothing to do on a computer a cable already has online.
    readonly property bool skipWifi: setupMode && FirstBoot.wifi.wiredOnline && FirstBoot.wifi.online

    function next() {
        if (step < stepNames.length) {
            step += setupMode && step === 1 && skipWifi ? 2 : 1;
        }
    }

    function finish() {
        if (setupMode) {
            try {
                WindowsImport.release();
                FirstBoot.finish({
                    home: Setup.plan(),
                    privacy: Privacy.plan(),
                    copy: WindowsImport.plan(),
                    programs: WindowsPrograms.plan()
                });
            } catch (error) {
                console.warn("Finishing setup failed:", error);
                FirstBoot.fail("Wild OS could not finish setting up. Restart the computer to try again.");
            }
        } else {
            WindowsPrograms.start();
            Setup.finish();
            Setup.quit();
        }
    }

    function back() {
        if (step > 1) {
            step -= setupMode && step === 3 && skipWifi ? 2 : 1;
        }
    }

    // The photo and the dim over it, drawn once and blurred again for the
    // part the sheet covers, the way the artboard's backdrop filter does.
    Item {
        id: backdrop
        anchors.fill: parent

        Image {
            id: photo

            readonly property real scale: implicitWidth > 0 ? Math.max(parent.width / implicitWidth, parent.height / implicitHeight) : 1

            width: implicitWidth * scale
            height: implicitHeight * scale
            x: (parent.width - width) / 2
            // object-position: center 45%
            y: (parent.height - height) * 0.45
            source: Setup.wallpaper
            asynchronous: true
            smooth: true
        }

        Rectangle {
            anchors.fill: parent
            color: UI.Style.scrim
        }
    }

    Item {
        id: sheetMask
        anchors.fill: parent
        visible: false
        layer.enabled: true

        Rectangle {
            x: sheet.x
            y: sheet.y
            width: sheet.width
            height: sheet.height
            radius: Metrics.radiusSheet
        }
    }

    Rectangle {
        id: shadowSource
        x: sheet.x
        y: sheet.y
        width: sheet.width
        height: sheet.height
        radius: Metrics.radiusSheet
        color: UI.Style.ground
        visible: false
        layer.enabled: true
    }

    MultiEffect {
        anchors.fill: shadowSource
        source: shadowSource
        shadowEnabled: true
        shadowColor: UI.Style.shadowFlyout
        shadowBlur: 1.0
        blurMax: 64
        shadowScale: 1.0
        shadowVerticalOffset: 40
        autoPaddingEnabled: true
    }

    MultiEffect {
        anchors.fill: backdrop
        source: backdrop
        blurEnabled: true
        blur: 1.0
        blurMax: 64
        saturation: 0.4
        // Padding would stretch the mask past the sheet's edges.
        autoPaddingEnabled: false
        maskEnabled: true
        maskSource: sheetMask
    }

    Item {
        id: sheet
        width: Metrics.sheetWidth
        // Grows with larger text, up to the screen, then the step scrolls.
        height: Math.min(root.height - 48, Math.max(Metrics.sheetHeight, stepLoader.item ? stepLoader.item.implicitHeight + 2 : 0, sidebar.implicitHeight + 2))
        anchors.centerIn: parent

        Rectangle {
            anchors.fill: parent
            radius: Metrics.radiusSheet
            color: UI.Style.glassWindowDeep
            border.width: 1
            border.color: UI.Style.borderGlass
        }

        Sidebar {
            id: sidebar
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.margins: 1
            width: Metrics.sidebarWidth
            steps: root.stepNames
            current: root.step
            showNote: root.stepNames[root.step - 1] === "Make it feel like home"
            canLeave: !root.setupMode
            onLater: Setup.quit()
        }

        Loader {
            id: stepLoader
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.left: sidebar.right
            anchors.margins: 1
            anchors.leftMargin: 0
            focus: true
            // Next slides in from the right and Back from the left, as the
            // Windows setup pages do.
            transform: UI.Reveal {
                item: stepLoader
                key: root.step
                directional: true
                dx: UI.Style.travelMedium
            }
            sourceComponent: (root.setupMode ? [regionStep, wifiStep, accountStep] : [])
                .concat([homeStep, filesStep, privacyStep, programsStep])[root.step - 1]
        }
    }

    Component {
        id: regionStep
        RegionStep {
            onNext: root.next()
        }
    }

    Component {
        id: wifiStep
        WifiStep {
            onNext: root.next()
            onBack: root.back()
        }
    }

    Component {
        id: accountStep
        AccountStep {
            onNext: root.next()
            onBack: root.back()
        }
    }

    Component {
        id: homeStep
        HomeStep {
            onNext: root.next()
            onBack: root.back()
            canGoBack: root.step > 1
        }
    }

    Component {
        id: filesStep
        FilesStep {
            onNext: root.next()
            onBack: root.back()
        }
    }

    Component {
        id: privacyStep
        PrivacyStep {
            onNext: root.next()
            onBack: root.back()
        }
    }

    Component {
        id: programsStep
        ProgramsStep {
            onBack: root.back()
            onFinished: root.finish()
        }
    }
}
