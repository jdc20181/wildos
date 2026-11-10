import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Templates as T
import QtQuick.Window

// A Wild OS dialog over a window: a title, a sentence or two, optional body
// content and buttons that say what they do. The glass is the design's one
// drawn dialog (WinApp.dc: radius 18, rgba(18,22,40,0.90), a 0.13 edge,
// 0 40px 110px shadow, dimmed backdrop rgba(6,10,26,0.35)). Buttons sit at
// the bottom right with the main action at the right-hand end, as the system
// dialog boards draw them (Cancel, then Save). The dialog moves primary
// buttons to the end itself, so callers may declare them in any order; set
// primaryLast false for the rare prompt drawn the other way round (the
// permission prompt's Yes, No).
//
// size "medium" (440 wide, 24 padding, 18px title, 13px text, 36px
// buttons) is for confirmations inside an app; "large" (640 wide, 30 / 32
// padding, 22px title, 14px text, 40px buttons on a footer band) is the
// WinApp install dialog and other system prompts.
T.Dialog {
    id: control

    property string text
    property string size: "medium"
    property string iconName
    default property alias body: extra.data
    property alias buttons: buttonRow.data
    property bool primaryLast: true

    readonly property bool large: size === "large"

    function holdsFocus(item) {
        for (let i = item; i; i = i.parent) {
            if (i === control.contentItem) {
                return true;
            }
        }
        return false;
    }

    // Enter presses the button that has the keyboard, otherwise the main
    // action, as Windows dialogs do. Fields that use Enter themselves accept
    // it first and never get here.
    function pressDefault(event) {
        const w = control.contentItem.Window.window;
        const f = w ? w.activeFocusItem : null;
        const shown = Array.from(buttonRow.children).filter(b => b.visible && b.enabled && b.clicked !== undefined);
        let target = shown.find(b => b === f);
        if (!target) {
            const primaries = shown.filter(b => b.primary === true);
            target = primaries[primaries.length - 1];
        }
        if (target) {
            target.clicked();
        } else {
            event.accepted = false;
        }
    }

    // Without this the keyboard can stay on whatever opened the dialog
    // behind it, and Enter never reaches the dialog.
    onOpened: {
        const w = contentItem.Window.window;
        if (!holdsFocus(w ? w.activeFocusItem : null)) {
            contentItem.forceActiveFocus(Qt.PopupFocusReason);
        }
    }

    parent: T.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(large ? 640 : 440, parent ? parent.width - 48 : 640)
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    modal: true
    focus: true
    padding: large ? 32 : 24
    topPadding: large ? 30 : 24
    bottomPadding: 0
    closePolicy: T.Popup.CloseOnEscape

    T.Overlay.modal: Rectangle {
        // Follows the window's rounded corners so no square of shade sits
        // outside them.
        radius: Window.window && Window.window.visibility !== Window.Maximized && Window.window.visibility !== Window.FullScreen ? Style.windowRadius : 0
        color: Style.scrim
        Behavior on opacity {
            NumberAnimation { duration: Style.motionFast }
        }
    }

    background: Rectangle {
        radius: Style.radiusDialog
        color: Style.glassDialog
        border.width: 1
        border.color: Style.borderDialog
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Style.shadowMenu
            shadowVerticalOffset: 24
            shadowBlur: 1.0
            blurMax: 48
        }
    }

    contentItem: ColumnLayout {
        spacing: control.large ? 18 : 12
        focus: true
        Keys.onReturnPressed: event => control.pressDefault(event)
        Keys.onEnterPressed: event => control.pressDefault(event)

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Icon {
                visible: control.iconName.length > 0
                name: control.iconName
                size: control.large ? 28 : 22
                color: Style.accent
            }

            Label {
                Layout.fillWidth: true
                text: control.title
                size: control.large ? 22 : 18
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                elide: Text.ElideNone
            }
        }

        Label {
            Layout.fillWidth: true
            visible: control.text.length > 0
            text: control.text
            size: control.large ? 14 : 13
            color: Style.inkStatus
            lineHeight: control.large ? 1.55 : 1.4
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        }

        ColumnLayout {
            id: extra
            Layout.fillWidth: true
            visible: children.length > 0
            spacing: 8
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: control.large ? 10 : 12
            Layout.leftMargin: -control.leftPadding
            Layout.rightMargin: -control.rightPadding
            Layout.preferredHeight: control.large ? 76 : 68
            bottomLeftRadius: Style.radiusDialog
            bottomRightRadius: Style.radiusDialog
            color: control.large ? (Style.dark ? Qt.rgba(4 / 255, 6 / 255, 18 / 255, 0.30) : Style.tone(0.04)) : "transparent"

            Rectangle {
                visible: control.large
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Style.borderSubtle
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: control.leftPadding
                anchors.rightMargin: control.rightPadding
                anchors.topMargin: control.large ? 0 : 4
                anchors.bottomMargin: control.large ? 0 : 20
                spacing: 8

                Item {
                    Layout.fillWidth: true
                }

                // A nested layout fills by default, which would leave a lone
                // button at the left.
                RowLayout {
                    id: buttonRow
                    Layout.fillWidth: false
                    spacing: 8

                    property bool ordering: false
                    function placePrimaryLast() {
                        if (!control.primaryLast || ordering) {
                            return;
                        }
                        ordering = true;
                        const kids = Array.from(children);
                        const primaries = kids.filter(c => c.primary === true);
                        // Only moves them when they are not already the last
                        // ones. Moving changes children and runs this again,
                        // so a dialog with two primaries (Files' questions)
                        // used to move them for ever, and every move took the
                        // keyboard off the button being clicked and cancelled
                        // the click.
                        const tail = kids.slice(kids.length - primaries.length);
                        if (primaries.some((b, i) => tail[i] !== b)) {
                            // Re-adding an item appends it, so this moves the
                            // primary buttons to the end in their own order.
                            for (const b of primaries) {
                                b.parent = null;
                                b.parent = buttonRow;
                            }
                        }
                        ordering = false;
                    }
                    onChildrenChanged: Qt.callLater(placePrimaryLast)
                    Component.onCompleted: placePrimaryLast()
                }
            }
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Style.motionFast; easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: Style.scaleEnter; to: 1; duration: Style.motionNormal; easing.type: Style.easeEnter }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Style.motionFast; easing.type: Style.easeExit }
    }
}
