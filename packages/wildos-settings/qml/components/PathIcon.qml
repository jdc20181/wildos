import QtQuick
import QtQuick.Shapes
import WildOS.UI as UI

// The few line icons Settings draws that WildOS.UI's set lacks (the status
// tick, Battery saver's leaf, the app window beside Last used, Sign-in
// options' row icons and fingerprint setup's), on the same 24px grid and
// round strokes as UI.Icon. Any other name falls through to UI.Icon, so a
// card can take either.
Item {
    id: icon

    property string name
    property color color: UI.Style.iconInk
    property real size: 16
    property real strokeWidth: size <= 16 ? 2 : 1.8

    readonly property var paths: ({
        "status-ok": "M2 12a10 10 0 1 0 20 0a10 10 0 1 0 -20 0ZM9 12l2 2 4-4",
        "status-alert": "M2 12a10 10 0 1 0 20 0a10 10 0 1 0 -20 0ZM12 7.5v5.5M12 16.5v.01",
        "leaf": "M11 20A7 7 0 0 1 9.8 6.1C15.5 5 17 4.48 19 2c1 2 2 4.18 2 8 0 5.5-4.78 10-10 10ZM2 21c0-3 1.85-5.36 5.08-6C9.5 14.52 12 13 13 12",
        "app-window": "M4 4h16a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2ZM2 8h20M6 4v4M10 4v4",
        "fingerprint": "M12 10a2 2 0 0 0-2 2c0 1.02-.1 2.51-.26 4M14 13.12c0 2.38 0 6.38-1 8.88M17.29 21.02c.12-.6.43-2.3.5-3.02M2 12a10 10 0 0 1 18-6M2 16h.01M21.8 16c.2-2 .131-5.354 0-6M5 19.5C5.5 18 6 15 6 12a6 6 0 0 1 .34-2M8.65 22c.21-.66.45-1.32.57-2M9 6.8a6 6 0 0 1 9 5.2v2",
        // Sign-in options' rows and the fingerprint setup dialog
        // (SignInOptions.dc, FingerprintSteps.dc).
        "key-round": "M2.586 17.414A2 2 0 0 0 2 18.828V21a1 1 0 0 0 1 1h3a1 1 0 0 0 1-1v-1a1 1 0 0 1 1-1h1a1 1 0 0 0 1-1v-1a1 1 0 0 1 1-1h.172a2 2 0 0 0 1.414-.586l.814-.814a6.5 6.5 0 1 0-4-4zM16.5 7.5h.01",
        "security-key": "M9 7a1 1 0 1 0 2 0a1 1 0 1 0 -2 0ZM3 20a1 1 0 1 0 2 0a1 1 0 1 0 -2 0ZM4.7 19.3 19 5M21 3l-3 1 2 2ZM9.26 7.68 5 12l2 5M10 14l5 2 3.5-3.5M18 12l1-1 1 1-1 1Z",
        "log-in": "M15 3h4a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2h-4M10 17l5-5-5-5M15 12H3",
        "shield-round": "M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z",
        "shield-round-check": "M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1zM9 12l2 2 4-4",
        "hard-drive": "M22 12H2M5.45 5.11 2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11zM6 16h.01M10 16h.01",
        "lock-round": "M5 11h14a2 2 0 0 1 2 2v7a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-7a2 2 0 0 1 2-2ZM7 11V7a5 5 0 0 1 10 0v4",
        "check": "M20 6 9 17l-5-5",
        // System > Recovery (RecoveryPage.dc, RecoveryGoBack.dc,
        // RecoveryReset.dc): restore points, reset, start-up options and
        // what goes back.
        "history": "M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8M3 3v5h5M12 7v5l4 2",
        "rotate-ccw": "M3 12a9 9 0 1 0 9-9 9.75 9.75 0 0 0-6.74 2.74L3 8M3 3v5h5",
        "power-round": "M12 2v10M18.4 6.6a9 9 0 1 1-12.77.04",
        "undo-2": "M9 14 4 9l5-5M4 9h10.5a5.5 5.5 0 0 1 5.5 5.5 5.5 5.5 0 0 1-5.5 5.5H11",
        "folder-check": "M20 20a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.9a2 2 0 0 1-1.69-.9L9.6 3.9A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2ZM9 13l2 2 4-4",
        "trash-round": "M3 6h18M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2M10 11v6M14 11v6",
        "x-mark": "M18 6 6 18M6 6l12 12",
        "plus-thin": "M5 12h14M12 5v14"
    })
    readonly property bool own: paths[name] !== undefined

    implicitWidth: size
    implicitHeight: size

    Shape {
        visible: icon.own
        width: 24
        height: 24
        preferredRendererType: Shape.CurveRenderer
        transform: Scale {
            xScale: icon.size / 24
            yScale: icon.size / 24
        }

        ShapePath {
            strokeColor: icon.color
            strokeWidth: icon.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg {
                path: icon.paths[icon.name] || ""
            }
        }
    }

    UI.Icon {
        visible: !icon.own
        name: icon.own ? "" : icon.name
        size: icon.size
        strokeWidth: icon.strokeWidth
        color: icon.color
    }
}
