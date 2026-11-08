pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Window
import WildOS.UI as UI
import WildOS.Screenshot

// Mark up, from the screenshot toast: draw on the capture with a pen or a
// highlighter, rub strokes out, then copy it or save it over the file in
// Pictures/Screenshots. Snipping Tool's ink tools, kept to what people
// reach for.
UI.AppWindow {
    id: win

    required property url file

    property string tool: "pen"
    property color ink: UI.Style.danger
    property var strokes: []
    property var undone: []
    property var drawing: null
    property bool dirty: false

    readonly property var swatches: [
        { name: "Red", colour: UI.Style.danger },
        { name: "Yellow", colour: UI.Style.warning },
        { name: "Blue", colour: UI.Style.accent },
        { name: "White", colour: UI.Style.inkOnWallpaper },
        { name: "Navy", colour: UI.Style.lakeNavy }
    ]

    deep: true
    title: "Mark up"
    width: Math.min(Math.max(720, picture.implicitWidth / Screen.devicePixelRatio + 48), Screen.desktopAvailableWidth - 80)
    height: Math.min(Math.max(520, picture.implicitHeight / Screen.devicePixelRatio + 160), Screen.desktopAvailableHeight - 80)
    minimumWidth: 640
    minimumHeight: 440

    function undo(): void {
        if (strokes.length === 0) {
            return;
        }
        const list = strokes.slice();
        undone = undone.concat([list.pop()]);
        strokes = list;
        dirty = true;
    }

    function redo(): void {
        if (undone.length === 0) {
            return;
        }
        const back = undone.slice();
        strokes = strokes.concat([back.pop()]);
        undone = back;
        dirty = true;
    }

    function erase(at: point): void {
        const reach = 10 / paper.fit;
        const kept = strokes.filter(s => !s.points.some(p => Math.abs(p.x - at.x) < reach + s.width / 2 && Math.abs(p.y - at.y) < reach + s.width / 2));
        if (kept.length !== strokes.length) {
            undone = [];
            strokes = kept;
            dirty = true;
        }
    }

    function render(then: var): void {
        paper.grabToImage(result => then(result), Qt.size(paper.width, paper.height));
    }

    function copy(): void {
        render(result => {
            ScreenshotApp.copyImage(result.image);
            toast.show("Copied with your marks");
        });
    }

    function save(): void {
        render(result => {
            if (result.saveToFile(win.file)) {
                ScreenshotApp.copyImageFile(win.file);
                toast.show("Saved and copied");
                win.dirty = false;
            } else {
                toast.show("Could not save to Pictures › Screenshots");
            }
        });
    }

    Shortcut { sequence: StandardKey.Undo; onActivated: win.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Y"]; onActivated: win.redo() }
    Shortcut { sequence: StandardKey.Save; onActivated: win.save() }
    Shortcut { sequence: StandardKey.Copy; onActivated: win.copy() }
    Shortcut { sequence: "Esc"; onActivated: win.close() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: win.maximised ? 0 : 1
        spacing: 0

        UI.TitleBar {
            Layout.fillWidth: true
            frame: win.frame
            title: win.title
        }

        // The tools: ink, colour, undo, then what to do with the result.
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 4

            UI.IconButton {
                tip: "Pen"
                iconName: "pencil"
                selected: win.tool === "pen"
                onClicked: win.tool = "pen"
            }
            UI.IconButton {
                tip: "Highlighter"
                glyph: "M9 15l-4 4h5l2-2M9 15l7.5-7.5a2.1 2.1 0 0 1 3 3L12 18M9 15l3 3"
                selected: win.tool === "highlighter"
                onClicked: win.tool = "highlighter"
            }
            UI.IconButton {
                tip: "Eraser"
                glyph: "M7 21h10M5.5 15.5l7-7a2 2 0 0 1 2.8 0l2.2 2.2a2 2 0 0 1 0 2.8L12 19H8.5l-3-3a.7.7 0 0 1 0-.5zM9 12l5 5"
                selected: win.tool === "eraser"
                onClicked: win.tool = "eraser"
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 24
                Layout.leftMargin: 6
                Layout.rightMargin: 6
                color: UI.Style.divider
            }

            Repeater {
                model: win.swatches

                delegate: UI.IconButton {
                    id: swatch
                    required property var modelData
                    tip: modelData.name
                    checked: Qt.colorEqual(win.ink, modelData.colour) && win.tool !== "eraser"
                    onClicked: {
                        win.ink = modelData.colour;
                        if (win.tool === "eraser") {
                            win.tool = "pen";
                        }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        radius: 9
                        color: swatch.modelData.colour
                        border.width: swatch.checked ? 2 : 1
                        border.color: swatch.checked ? UI.Style.ink : UI.Style.borderControl
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 24
                Layout.leftMargin: 6
                Layout.rightMargin: 6
                color: UI.Style.divider
            }

            UI.IconButton {
                tip: "Undo (Ctrl+Z)"
                iconName: "back"
                enabled: win.strokes.length > 0
                onClicked: win.undo()
            }
            UI.IconButton {
                tip: "Redo (Ctrl+Y)"
                iconName: "forward"
                enabled: win.undone.length > 0
                onClicked: win.redo()
            }

            Item { Layout.fillWidth: true }

            UI.Button {
                text: "Copy"
                iconName: "copy"
                onClicked: win.copy()
            }
            UI.Button {
                kind: "primary"
                text: "Save"
                onClicked: win.save()
            }
        }

        // The picture on a sunken ground, scaled to fit, marks on top.
        Rectangle {
            id: stage
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: UI.Style.surfaceSunken
            bottomLeftRadius: win.maximised ? 0 : UI.Style.windowRadius - 1
            bottomRightRadius: bottomLeftRadius
            clip: true

            Image {
                id: picture
                visible: false
                source: win.file
                cache: false
            }

            // At the picture's own pixel size, so saving keeps every pixel.
            Item {
                id: paper
                readonly property real fit: Math.min(1, (stage.width - 48) / Math.max(1, width), (stage.height - 48) / Math.max(1, height))
                width: picture.implicitWidth
                height: picture.implicitHeight
                x: Math.round((stage.width - width * fit) / 2)
                y: Math.round((stage.height - height * fit) / 2)
                transform: Scale { xScale: paper.fit; yScale: paper.fit }

                Image {
                    anchors.fill: parent
                    source: win.file
                    cache: false
                    smooth: true
                }

                Repeater {
                    model: win.drawing ? win.strokes.concat([win.drawing]) : win.strokes

                    delegate: Shape {
                        id: stroke
                        required property var modelData
                        anchors.fill: parent
                        opacity: modelData.tool === "highlighter" ? 0.4 : 1
                        preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            strokeColor: stroke.modelData.colour
                            strokeWidth: stroke.modelData.width
                            fillColor: "transparent"
                            capStyle: stroke.modelData.tool === "highlighter" ? ShapePath.SquareCap : ShapePath.RoundCap
                            joinStyle: ShapePath.RoundJoin
                            PathPolyline { path: stroke.modelData.points }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.CrossCursor
                    onPressed: mouse => {
                        if (win.tool === "eraser") {
                            win.erase(Qt.point(mouse.x, mouse.y));
                            return;
                        }
                        // Line widths are in screen pixels, whatever the scale.
                        const scale = picture.implicitWidth / Math.max(1, picture.implicitWidth / Screen.devicePixelRatio);
                        win.drawing = {
                            tool: win.tool,
                            colour: win.ink,
                            width: (win.tool === "highlighter" ? 16 : 4) * scale,
                            points: [Qt.point(mouse.x, mouse.y)]
                        };
                    }
                    onPositionChanged: mouse => {
                        if (win.tool === "eraser") {
                            win.erase(Qt.point(mouse.x, mouse.y));
                            return;
                        }
                        if (!win.drawing) {
                            return;
                        }
                        const d = win.drawing;
                        win.drawing = { tool: d.tool, colour: d.colour, width: d.width, points: d.points.concat([Qt.point(mouse.x, mouse.y)]) };
                    }
                    onReleased: {
                        if (win.drawing) {
                            const d = win.drawing;
                            win.drawing = null;
                            win.strokes = win.strokes.concat([d.points.length > 1 ? d
                                : { tool: d.tool, colour: d.colour, width: d.width, points: [d.points[0], Qt.point(d.points[0].x + 0.5, d.points[0].y)] }]);
                            win.undone = [];
                            win.dirty = true;
                        }
                    }
                }
            }

            UI.Toast {
                id: toast
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 20
            }
        }
    }
}
