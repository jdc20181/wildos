import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasma5support as P5Support
import org.kde.kirigami as Kirigami
import WildOS.UI as UI

// The Task view button beside Start: opens KWin's Overview, which shows every
// window and the virtual desktops, as Win+Tab does. It shares Start's 46 px
// square, hover fill and press dip so the two read as one pair.
PlasmoidItem {
    id: root

    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    Plasmoid.icon: UI.Style.dark ? "wildos-task-view" : "wildos-task-view-light"
    preferredRepresentation: fullRepresentation
    toolTipMainText: "Task view"
    toolTipSubText: "Win+Tab"

    Layout.minimumWidth: 46
    Layout.maximumWidth: 46

    // KWin's Overview is a global shortcut; invoking it through the shortcut
    // service toggles it exactly as the keys would.
    function toggleOverview(): void {
        runner.connectSource("dbus-send --session --type=method_call --dest=org.kde.kglobalaccel "
            + "/component/kwin org.kde.kglobalaccel.Component.invokeShortcut string:Overview");
    }

    P5Support.DataSource {
        id: runner
        engine: "executable"
        onNewData: source => disconnectSource(source)
    }

    fullRepresentation: MouseArea {
        id: button

        Layout.minimumWidth: 46
        Layout.preferredWidth: 46
        Layout.minimumHeight: 46
        Layout.preferredHeight: 46

        hoverEnabled: true
        Accessible.name: "Task view"
        Accessible.role: Accessible.Button
        onClicked: root.toggleOverview()

        Rectangle {
            anchors.centerIn: parent
            width: 46
            height: 46
            radius: 13
            color: button.containsMouse ? UI.Style.surfaceHover : "transparent"
            Behavior on color {
                ColorAnimation { duration: UI.Style.hoverDuration }
            }
        }

        property real glyphScale: 1

        onPressedChanged: {
            if (pressed && Kirigami.Units.longDuration > 1) {
                dip.restart();
            }
        }

        SequentialAnimation {
            id: dip
            NumberAnimation { target: button; property: "glyphScale"; to: 0.86; duration: 35; easing.type: Easing.OutCubic }
            NumberAnimation { target: button; property: "glyphScale"; to: 1.04; duration: 35; easing.type: Easing.OutCubic }
            NumberAnimation { target: button; property: "glyphScale"; to: 1; duration: 30; easing.type: Easing.InOutQuad }
        }

        Kirigami.Icon {
            anchors.centerIn: parent
            width: 26
            height: 26
            roundToIconSize: false
            source: Plasmoid.icon
            scale: button.glyphScale
        }
    }
}
