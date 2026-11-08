import QtQuick
import WildOS.UI as UI

// Takes the place of an app's button while it installs, updates or is
// removed: the button's own shape, filling with the accent as it goes, with
// the step and percentage written on it.
Rectangle {
    id: pill

    property var app
    property int fontSize: 12

    readonly property bool known: app && app.progress >= 0
    readonly property string label: {
        if (!app) {
            return "";
        }
        if (app.queued) {
            return "Waiting";
        }
        const pct = known ? " " + app.progress + "%" : "";
        if (app.phase === "auth") {
            return "Waiting for password";
        }
        if (app.action === "remove") {
            return "Removing" + pct;
        }
        if (app.action === "update") {
            return "Updating" + pct;
        }
        if (app.phase === "download") {
            return "Downloading" + pct;
        }
        if (app.phase === "install") {
            return "Installing" + pct;
        }
        return known && app.progress > 0 ? "Installing" + pct : "Getting ready";
    }

    implicitWidth: Math.max(120, text.implicitWidth + 32)
    implicitHeight: Math.max(32, Math.ceil(text.implicitHeight) + 12)
    radius: UI.Style.radiusSm
    color: UI.Style.surfaceControl
    border.width: 1
    border.color: UI.Style.accentBorderStrong
    clip: true
    Accessible.role: Accessible.ProgressBar
    Accessible.name: label

    Rectangle {
        id: fill
        visible: pill.known
        x: 1
        y: 1
        height: parent.height - 2
        radius: pill.radius - 1
        width: Math.max(0, (parent.width - 2) * (pill.app ? pill.app.progress : 0) / 100)
        color: UI.Style.accentSelected
        Behavior on width {
            NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
        }
    }

    // Before PackageKit or Flathub know how much there is to do, a short
    // band sweeps across instead of a stalled bar.
    Rectangle {
        id: sweep
        visible: !pill.known && pill.app && !pill.app.queued
        y: 1
        height: parent.height - 2
        width: parent.width * 0.35
        radius: pill.radius - 1
        color: UI.Style.accentSelected
        NumberAnimation on x {
            running: sweep.visible
            loops: Animation.Infinite
            from: -sweep.width
            to: pill.width
            duration: 1300
            easing.type: Easing.InOutQuad
        }
    }

    UI.Label {
        id: text
        anchors.centerIn: parent
        text: pill.label
        size: pill.fontSize
        font.weight: Font.DemiBold
        font.features: { "tnum": 1 }
    }
}
