import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI
import org.wildos.shell as WildOS

// The taskbar item from board 4.13: the window-with-shield mark that
// Settings uses for Windows apps, its status dot, and two lines, "Windows
// mode" and the state. A 40 px button with 10 px corners, 10 px in on the
// left and 12 on the right. It sits at the left end of the bar in both
// styles and carries its own gap from the screen edge, so the bar needs no
// spacer before it: 8 px docked; 13 px floating, where its pill starts 5 px
// in, so with the bar's own 12 px margin the pill sits 17 px from the left
// edge, as the status pill does from the right, with 8 px inside. It takes no room at all
// until Windows mode is set up or when it is turned off.
MouseArea {
    id: chip

    required property var winmode

    readonly property int edge: WildOS.TaskbarStyle.floating ? 13 : 8
    property bool wasExpanded: false

    readonly property real fullWidth: edge + button.width
    Layout.minimumWidth: winmode.shown ? fullWidth : 0
    Layout.preferredWidth: Layout.minimumWidth
    Layout.maximumWidth: Layout.minimumWidth
    Layout.minimumHeight: winmode.shown ? 40 : 0
    // Plasma shows the compact representation itself whatever it says, so
    // hiding is no width and nothing drawn.
    enabled: winmode.shown

    hoverEnabled: true
    Accessible.role: Accessible.Button
    Accessible.name: "Windows mode: " + winmode.stateText
    Accessible.description: winmode.toolTipSubText

    // Remembering the state at press time stops a click that closes the popup
    // (it loses focus first) from opening it again straight away.
    onPressed: wasExpanded = winmode.expanded
    onClicked: winmode.expanded = !wasExpanded

    Rectangle {
        id: button
        visible: chip.winmode.shown
        x: chip.edge
        anchors.verticalCenter: parent.verticalCenter
        width: row.implicitWidth + 22
        height: 40
        radius: 10
        color: chip.pressed ? UI.Style.surfacePressed
            : chip.winmode.expanded || chip.containsMouse ? UI.Style.tone(0.12) : UI.Style.tone(0.07)
        border.width: 1
        border.color: chip.winmode.expanded ? UI.Style.tone(0.10) : "transparent"
        Behavior on color {
            ColorAnimation { duration: UI.Style.hoverDuration }
        }

        RowLayout {
            id: row
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            Item {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20

                UI.Icon {
                    anchors.fill: parent
                    name: "windowsapps"
                    size: 20
                    strokeWidth: 1.8
                    color: UI.Style.ink
                }

                // A ring of the bar's colour around the dot keeps it apart
                // from the mark's stroke underneath.
                Rectangle {
                    x: 20 + 3 - width
                    y: 20 + 2 - height
                    width: 12
                    height: 12
                    radius: 6
                    color: Tones.bar

                    StatusDot {
                        anchors.centerIn: parent
                        level: chip.winmode.level
                    }
                }
            }

            ColumnLayout {
                spacing: 1

                UI.Label {
                    text: "Windows mode"
                    size: 12
                    font.weight: Font.DemiBold
                    lineHeight: 1.2
                }

                UI.Label {
                    text: chip.winmode.stateText
                    size: 11
                    lineHeight: 1.2
                    color: chip.winmode.level === "attention" ? Tones.dot("attention") : UI.Style.inkMuted
                }
            }
        }
    }
}
