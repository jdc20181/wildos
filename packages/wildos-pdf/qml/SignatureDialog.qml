import QtQuick
import QtQuick.Layouts
import WildOS.PDF
import WildOS.UI as UI

// Making a signature: drawn with the mouse, a pen or a finger in a box
// three times wider than tall, or typed and set in a handwriting font.
// Either way it goes to the keyring (Signatures).
UI.Dialog {
    id: dialog

    // "draw" or "type"
    property string mode: "draw"
    property var strokes: []
    property var current: []
    readonly property bool ready: mode === "draw" ? strokes.length > 0 : nameField.text.trim().length > 0

    signal added(string id)

    function start(how) {
        mode = how;
        strokes = [];
        current = [];
        nameField.text = "";
        open();
        if (how === "type") {
            nameField.forceActiveFocus();
        }
    }
    function keep() {
        const id = mode === "draw" ? Signatures.addDrawn(strokes, pad.width / pad.height) : Signatures.addTyped(nameField.text);
        close();
        if (id.length > 0) {
            added(id);
        }
    }

    title: mode === "draw" ? qsTr("Draw your signature") : qsTr("Type your signature")
    text: mode === "draw" ? qsTr("Sign in the box with your mouse, pen or finger.") : qsTr("Your name is set in a handwriting style.")

    UI.TextField {
        id: nameField
        Layout.fillWidth: true
        visible: dialog.mode === "type"
        placeholderText: qsTr("Your name")
        maximumLength: 60
        onAccepted: if (dialog.ready) dialog.keep()
    }

    // White like the paper it will go on.
    Rectangle {
        id: pad
        Layout.fillWidth: true
        Layout.preferredHeight: Math.round(width / 3)
        radius: UI.Style.radiusSm
        color: "#ffffff"
        border.width: 1
        border.color: UI.Style.borderControl
        clip: true

        Rectangle {
            x: 24
            width: parent.width - 48
            y: parent.height * 0.72
            height: 1
            color: "#c9ced8"
            visible: dialog.mode === "draw"
        }

        Text {
            anchors.centerIn: parent
            width: parent.width - 32
            visible: dialog.mode === "type"
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: nameField.text
            font.family: "Dancing Script"
            font.pixelSize: 46
            color: "#1f2a6b"
        }

        Canvas {
            id: ink
            anchors.fill: parent
            visible: dialog.mode === "draw"
            antialiasing: true
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                ctx.lineCap = "round";
                ctx.lineJoin = "round";
                ctx.lineWidth = width * 0.0055;
                ctx.strokeStyle = "#1f2a6b";
                const all = dialog.strokes.concat(dialog.current.length > 0 ? [dialog.current] : []);
                for (const line of all) {
                    if (line.length < 2) {
                        continue;
                    }
                    ctx.beginPath();
                    ctx.moveTo(line[0].x * width, line[0].y * height);
                    for (let i = 1; i < line.length; ++i) {
                        ctx.lineTo(line[i].x * width, line[i].y * height);
                    }
                    ctx.stroke();
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: dialog.mode === "draw"
            cursorShape: Qt.CrossCursor
            function at(mouse) {
                return Qt.point(Math.max(0, Math.min(1, mouse.x / width)), Math.max(0, Math.min(1, mouse.y / height)));
            }
            onPressed: mouse => {
                dialog.current = [at(mouse)];
            }
            onPositionChanged: mouse => {
                if (!pressed) {
                    return;
                }
                dialog.current = dialog.current.concat([at(mouse)]);
                ink.requestPaint();
            }
            onReleased: {
                if (dialog.current.length > 1) {
                    dialog.strokes = dialog.strokes.concat([dialog.current]);
                }
                dialog.current = [];
                ink.requestPaint();
            }
        }

        UI.Button {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 6
            visible: dialog.mode === "draw" && dialog.strokes.length > 0
            kind: "text"
            size: "small"
            text: qsTr("Clear")
            // On the white pad in either scheme.
            textColor: "#1f2633"
            onClicked: {
                dialog.strokes = [];
                ink.requestPaint();
            }
        }
    }

    buttons: [
        UI.Button {
            text: qsTr("Cancel")
            onClicked: dialog.close()
        },
        UI.Button {
            kind: "primary"
            text: qsTr("Add signature")
            enabled: dialog.ready
            onClicked: dialog.keep()
        }
    ]
}
