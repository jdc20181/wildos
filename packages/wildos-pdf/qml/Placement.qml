import QtQuick
import QtQuick.Shapes
import WildOS.PDF
import WildOS.UI as UI

// A signature, date or set of initials put on a page, before Save a signed
// copy writes it in (AppPdf.dc: a dashed accent frame with a square handle
// at each corner). Drag it to move it; drag a corner to size it, which keeps
// its shape. Delete or the cross takes it off.
Item {
    id: item

    property var placement
    property var app
    property var pageItem
    readonly property bool selected: app.selectedPlacement === placement.uid
    readonly property bool isDate: placement.kind === "date"
    readonly property real aspect: placement.width * pageItem.width / Math.max(1, placement.height * pageItem.height)

    x: placement.x * pageItem.width
    y: placement.y * pageItem.height
    width: placement.width * pageItem.width
    height: placement.height * pageItem.height
    z: selected ? 2 : 1

    function commit(nx, ny, nw, nh) {
        app.updatePlacement(placement.uid, {
            x: Math.max(0, Math.min(1 - nw / pageItem.width, nx / pageItem.width)),
            y: Math.max(0, Math.min(1 - nh / pageItem.height, ny / pageItem.height)),
            width: nw / pageItem.width,
            height: nh / pageItem.height
        });
    }

    Image {
        anchors.fill: parent
        visible: !item.isDate
        source: item.placement.kind === "initials" ? "image://signature/initials/" + Signatures.revision
            : "image://signature/" + item.placement.signature + "/" + Signatures.revision
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        cache: false
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 2 * item.pageItem.pointScale
        anchors.verticalCenter: parent.verticalCenter
        visible: item.isDate
        text: item.placement.text ?? ""
        font.family: "Liberation Sans"
        font.pixelSize: (item.placement.pointSize ?? 10.5) * item.pageItem.pointScale
        color: "#000000"
    }

    MouseArea {
        id: body
        anchors.fill: parent
        preventStealing: true
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        property point grab
        property point origin
        onPressed: mouse => {
            item.app.selectedPlacement = item.placement.uid;
            item.app.focusPages();
            grab = mapToItem(item.pageItem, mouse.x, mouse.y);
            origin = Qt.point(item.x, item.y);
        }
        onPositionChanged: mouse => {
            if (!pressed) {
                return;
            }
            const now = mapToItem(item.pageItem, mouse.x, mouse.y);
            item.x = origin.x + now.x - grab.x;
            item.y = origin.y + now.y - grab.y;
        }
        onReleased: item.commit(item.x, item.y, item.width, item.height)
    }

    Shape {
        anchors.fill: parent
        anchors.margins: -6
        visible: item.selected
        ShapePath {
            strokeColor: UI.Style.accent
            strokeWidth: 1.5
            strokeStyle: ShapePath.DashLine
            dashPattern: [3, 2]
            fillColor: "transparent"
            PathRectangle {
                x: 0.75
                y: 0.75
                width: item.width + 12 - 1.5
                height: item.height + 12 - 1.5
                radius: 4
            }
        }
    }

    // The corners: each keeps the opposite corner where it is.
    Repeater {
        model: item.isDate ? [] : [0, 1, 2, 3]
        delegate: Rectangle {
            id: handle
            required property int modelData
            readonly property bool atRight: modelData === 1 || modelData === 3
            readonly property bool atBottom: modelData >= 2
            visible: item.selected
            x: (atRight ? item.width + 6 : -6) - 4
            y: (atBottom ? item.height + 6 : -6) - 4
            width: 8
            height: 8
            radius: 2
            color: "#ffffff"
            border.width: 1.5
            border.color: UI.Style.accent

            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                preventStealing: true
                cursorShape: handle.atRight === handle.atBottom ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                property rect start
                property point grab
                onPressed: mouse => {
                    start = Qt.rect(item.x, item.y, item.width, item.height);
                    grab = mapToItem(item.pageItem, mouse.x, mouse.y);
                }
                onPositionChanged: mouse => {
                    if (!pressed) {
                        return;
                    }
                    const now = mapToItem(item.pageItem, mouse.x, mouse.y);
                    const dx = handle.atRight ? now.x - grab.x : grab.x - now.x;
                    const w = Math.max(24, start.width + dx);
                    const h = w / item.aspect;
                    item.width = w;
                    item.height = h;
                    item.x = handle.atRight ? start.x : start.x + start.width - w;
                    item.y = handle.atBottom ? start.y : start.y + start.height - h;
                }
                onReleased: item.commit(item.x, item.y, item.width, item.height)
            }
        }
    }

    // On the menu glass, so it reads on the white page in either scheme.
    Rectangle {
        visible: item.selected
        x: item.width + 10
        y: -18
        width: 26
        height: 26
        radius: 13
        color: UI.Style.glassMenu
        border.width: 1
        border.color: UI.Style.borderMenu

        UI.IconButton {
            anchors.fill: parent
            radius: 13
            iconName: "close"
            iconSize: 12
            tip: item.isDate ? qsTr("Remove the date") : qsTr("Remove")
            onClicked: item.app.removePlacement(item.placement.uid)
        }
    }
}
