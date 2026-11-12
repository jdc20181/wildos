import QtQuick
import QtQuick.Window
import Qt.labs.folderlistmodel
import Qt5Compat.GraphicalEffects
import org.kde.plasma.plasmoid
import org.kde.taskmanager as TaskManager
import org.wildos.shell as WildOS

// Overclock's wallpaper: the picture as Plasma's Image wallpaper draws it,
// then the W at the centre of the screen, outlined in the taskbar
// underglow's flowing spectrum. The W is drawn live rather than baked into
// the picture so it stays sharp at every resolution and can move. The
// picture under it slowly breathes in and out.
WallpaperItem {
    id: root

    // As for org.kde.image: a picture file or a wallpaper package folder,
    // as a path or a file URL.
    readonly property string configuredImage: String(root.configuration.Image || "")
    readonly property string imagePath: configuredImage.startsWith("file://") ? decodeURIComponent(configuredImage.substring(7)) : configuredImage
    readonly property bool isPackage: imagePath !== "" && !/\.(jpe?g|png|webp|avif|jxl|svg)$/i.test(imagePath)
    readonly property size wanted: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
    // The picture breathes out to 8% larger, so it is decoded at that size
    // and the streaks stay one pixel to one pixel at the deepest breath.
    readonly property size decoded: Qt.size(Math.ceil(wanted.width * breath.deepest), Math.ceil(wanted.height * breath.deepest))

    // A package holds the picture at several sizes named WIDTHxHEIGHT. The
    // smallest that covers the screen is as sharp as any and costs the
    // least memory; failing that, the largest.
    readonly property string pictureUrl: {
        if (!isPackage) {
            return imagePath === "" ? "" : "file://" + imagePath;
        }
        let best = "";
        let bestArea = 0;
        let largest = "";
        let largestArea = -1;
        for (let i = 0; i < sizes.count; ++i) {
            const name = String(sizes.get(i, "fileName"));
            const match = /^(\d+)x(\d+)/.exec(name);
            const w = match ? Number(match[1]) : 0;
            const h = match ? Number(match[2]) : 0;
            if (w >= decoded.width && h >= decoded.height && (best === "" || w * h < bestArea)) {
                best = name;
                bestArea = w * h;
            }
            if (w * h > largestArea) {
                largest = name;
                largestArea = w * h;
            }
        }
        const chosen = best !== "" ? best : largest;
        return chosen === "" ? "" : "file://" + imagePath + "/contents/images/" + chosen;
    }

    // The same pauses as the underglow: Reduce motion, battery and a full
    // screen window. The spectrum then holds still, still in colour. It also
    // rests while nobody can see this screen.
    readonly property bool still: WildOS.Underglow.reducedMotion || WildOS.Underglow.onBattery || fullScreen || unseen

    // The screen is locked or switched off, or a window fills it.
    readonly property bool unseen: WildOS.Session.locked || WildOS.Session.screensOff.indexOf(Screen.name) !== -1 || covered

    // The picture breathes unless the user turned animations off. It holds
    // where it is, rather than settling, while nobody can see it and on
    // battery saver, so it picks up again without a jump.
    readonly property bool breathes: !WildOS.Underglow.reducedMotion
    readonly property bool breathHeld: unseen || WildOS.Session.powerSaver

    // Only KWin knows which windows are full screen, and it tells its own
    // process, where the underglow runs. Out here in plasmashell the task
    // manager's model gives the same answer.
    property bool fullScreen: false
    // A window maximised or full screen on this screen, which hides all of
    // the picture but the strip behind the taskbar.
    property bool covered: false

    // Where the spectrum's slide is, from the wall clock the underglow also
    // reads, so the W and the taskbar show the same colours at once.
    property real phase: WildOS.Underglow.flowPhase()

    function countFullScreen(): void {
        let anyFullScreen = false;
        let here = false;
        for (let i = 0; i < fullScreenWindows.count; ++i) {
            const window = fullScreenWindows.objectAt(i);
            if (!window) {
                continue;
            }
            anyFullScreen = anyFullScreen || window.covering;
            here = here || window.coveringHere;
        }
        fullScreen = anyFullScreen;
        covered = here;
    }

    FolderListModel {
        id: sizes
        folder: root.isPackage ? "file://" + root.imagePath + "/contents/images" : ""
        nameFilters: ["*.jpg", "*.jpeg", "*.png", "*.webp", "*.avif", "*.jxl"]
        showDirs: false
    }

    TaskManager.VirtualDesktopInfo {
        id: virtualDesktops
    }

    TaskManager.ActivityInfo {
        id: activities
    }

    Instantiator {
        id: fullScreenWindows

        model: TaskManager.TasksModel {
            virtualDesktop: virtualDesktops.currentDesktop
            activity: activities.currentActivity
            filterByVirtualDesktop: true
            filterByActivity: true
            filterByScreen: false
            groupMode: TaskManager.TasksModel.GroupDisabled
        }
        delegate: QtObject {
            readonly property bool covering: model.IsFullScreen === true && model.IsMinimized !== true
            // The task manager names the screen a window is on by its
            // geometry, in the same logical pixels as Screen.
            readonly property bool coveringHere: (model.IsFullScreen === true || model.IsMaximized === true) && model.IsMinimized !== true
                && model.ScreenGeometry !== undefined
                && model.ScreenGeometry.x === root.Screen.virtualX && model.ScreenGeometry.y === root.Screen.virtualY
                && model.ScreenGeometry.width === root.Screen.width && model.ScreenGeometry.height === root.Screen.height
            onCoveringChanged: Qt.callLater(root.countFullScreen)
            onCoveringHereChanged: Qt.callLater(root.countFullScreen)
        }
        // Later, so a window on its way out is no longer counted.
        onObjectAdded: Qt.callLater(root.countFullScreen)
        onObjectRemoved: Qt.callLater(root.countFullScreen)
    }

    // Holds back the splash screen until the picture is up, as the stock
    // wallpaper does.
    Binding {
        target: root
        property: "loading"
        value: picture.status === Image.Loading || sizes.status === FolderListModel.Loading
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    // The picture alone breathes, eased in for 14 seconds and back out for
    // 14 about the centre of the screen; the W above stays put. The scale
    // comes from how long it has breathed, not from the clock, so a pause
    // holds the frame and the breath carries on from there. 1 - cos is
    // InOutSine run forwards and back, so it never jumps at either end.
    Item {
        id: breath

        readonly property real deepest: 1.08
        readonly property real cycle: 28
        // Seconds breathed, kept within one cycle.
        property real elapsed: 0

        anchors.fill: parent
        transformOrigin: Item.Center

        function scaleAt(t: real): real {
            return 1 + (deepest - 1) * (1 - Math.cos(2 * Math.PI * t / cycle)) / 2;
        }

        Image {
            id: picture
            anchors.fill: parent
            source: root.pictureUrl
            fillMode: Image.PreserveAspectCrop
            sourceSize: root.decoded
            asynchronous: true
            cache: false
            smooth: true
            // Scaled by fractions of a pixel, the thin streaks would shimmer
            // without it.
            mipmap: true
        }

        FrameAnimation {
            running: root.breathes && root.width > 0
            paused: root.breathHeld
            onTriggered: {
                // Capped, so a long gap between frames (a stall, or the
                // compositor not drawing) is not a jump.
                breath.elapsed = (breath.elapsed + Math.min(frameTime, 0.1)) % breath.cycle;
                const next = breath.scaleAt(breath.elapsed);
                // The edge of the screen moves at most about 9px a second
                // on a 1920px screen. Redrawing once it has gone a quarter
                // of a pixel looks the same as every frame and spares the
                // GPU most of them.
                if (Math.abs(next - breath.scale) * Math.max(root.wanted.width, root.wanted.height) / 2 >= 0.25) {
                    breath.scale = next;
                }
            }
            // Animations off: stand still at the picture's own size.
            onRunningChanged: {
                if (!running) {
                    breath.elapsed = 0;
                    breath.scale = 1;
                }
            }
        }
    }

    Item {
        id: mark

        // One unit of the logo's 143.5-unit-wide viewBox. The viewBox is
        // 7.8% of the screen's width, 112px on a 1440px screen.
        readonly property real unit: root.width * 0.078 / 143.5
        // Room around the path's bounds (x 23.66 to 162, y 17 to 112.28)
        // for the mitred tips and the glow's blur.
        readonly property real pad: 8

        width: (138.34 + pad * 2) * unit
        height: (95.28 + pad * 2) * unit
        anchors.centerIn: parent
        visible: root.width > 0 && root.height > 0

        // The spectrum repeats once across the viewBox, starting at its left
        // edge, and slides right one viewBox width per cycle as the
        // underglow slides one taskbar width. Four copies cover the layer
        // wherever the slide is.
        Item {
            id: spectrum

            readonly property real tile: 143.5 * mark.unit
            readonly property real start: (mark.pad - 2.58) * mark.unit

            anchors.fill: parent
            visible: false
            layer.enabled: true
            clip: true

            Row {
                x: spectrum.start - spectrum.tile * (2 - root.phase)

                Repeater {
                    model: 4
                    delegate: Rectangle {
                        width: spectrum.tile
                        height: spectrum.height
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0 / 7; color: "#ff3d6e" }
                            GradientStop { position: 1 / 7; color: "#ffb03d" }
                            GradientStop { position: 2 / 7; color: "#f5ff3d" }
                            GradientStop { position: 3 / 7; color: "#3dff7a" }
                            GradientStop { position: 4 / 7; color: "#3dd5ff" }
                            GradientStop { position: 5 / 7; color: "#5c7cff" }
                            GradientStop { position: 6 / 7; color: "#c85cff" }
                            GradientStop { position: 7 / 7; color: "#ff3d6e" }
                        }
                    }
                }
            }
        }

        Outline {
            id: core
            anchors.fill: parent
            unit: mark.unit
            pad: mark.pad
            lineWidth: 1.8
        }

        Outline {
            id: halo
            anchors.fill: parent
            unit: mark.unit
            pad: mark.pad
            lineWidth: 3
        }

        OpacityMask {
            id: haloColour
            anchors.fill: parent
            source: spectrum
            maskSource: halo
            visible: false
        }

        // The glow under the line, tight rather than hazy.
        GaussianBlur {
            anchors.fill: parent
            source: haloColour
            deviation: 1.3 * mark.unit
            radius: Math.ceil(deviation * 3)
            samples: radius * 2 + 1
            opacity: 0.5
        }

        OpacityMask {
            anchors.fill: parent
            source: spectrum
            maskSource: core
        }
    }

    // The W slides about 19px a second on a 1440px screen. Moving it only
    // once the spectrum has gone half a pixel spares the desktop a redraw on
    // frames where nothing visible would change.
    FrameAnimation {
        running: !root.still && mark.visible
        onTriggered: {
            const next = WildOS.Underglow.flowPhase();
            if (Math.abs(next - root.phase) * spectrum.tile >= 0.5) {
                root.phase = next;
            }
        }
    }
}
