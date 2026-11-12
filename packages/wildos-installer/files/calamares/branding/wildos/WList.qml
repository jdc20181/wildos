/* Single-choice list in a card, used for languages, time zones and keyboard
 * layouts. The parent supplies the model and names the roles that hold the
 * visible text ("modelData" for a plain string list); picking a row emits
 * picked(index). Colours, hover and the focus ring are WildOS.UI's, the
 * ring only after the keyboard has been used, as everywhere in Wild OS.
 */
import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

Rectangle {
    id: card

    property alias model: list.model
    property alias currentIndex: list.currentIndex
    property alias view: list
    property string textRole: "label"
    property string detailRole: ""
    signal picked(int index)
    property bool keyboardUsed: false

    radius: UI.Style.radiusMd
    color: UI.Style.surfaceRaised
    border.width: 1
    border.color: UI.Style.borderSubtle

    function roleText(rowModel, role) {
        if (role === "")
            return "";
        var value = rowModel[role];
        return (value === undefined || value === null) ? "" : String(value);
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: 6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        highlightMoveDuration: 0
        keyNavigationEnabled: true
        activeFocusOnTab: true
        currentIndex: -1
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

        delegate: Rectangle {
            id: row
            required property int index
            required property var model
            readonly property bool selected: ListView.isCurrentItem

            width: ListView.view.width - 10
            height: 38
            radius: UI.Style.radiusSm
            color: selected ? UI.Style.accentSelected
                            : (hover.containsMouse ? UI.Style.surfaceHover : "transparent")

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: card.roleText(row.model, card.textRole)
                    elide: Text.ElideRight
                    color: UI.Style.ink
                    font.family: "Albert Sans"
                    font.pixelSize: 13
                    font.weight: row.selected ? UI.Style.weightSemiBold : UI.Style.weightRegular
                }
                Text {
                    visible: text !== ""
                    text: card.roleText(row.model, card.detailRole)
                    elide: Text.ElideRight
                    color: UI.Style.inkMuted
                    font.family: "Albert Sans"
                    font.pixelSize: 12
                }
                Canvas {
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    visible: row.selected
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();
                        ctx.scale(width / 24, height / 24);
                        ctx.strokeStyle = UI.Style.accentText;
                        ctx.lineWidth = 2.6;
                        ctx.lineCap = "round";
                        ctx.lineJoin = "round";
                        ctx.beginPath();
                        ctx.moveTo(5, 12);
                        ctx.lineTo(10, 17);
                        ctx.lineTo(19, 7);
                        ctx.stroke();
                    }
                }
            }

            MouseArea {
                id: hover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    card.keyboardUsed = false;
                    list.currentIndex = row.index;
                    card.picked(row.index);
                }
            }
        }

        Keys.onUpPressed: { card.keyboardUsed = true; decrementCurrentIndex(); card.picked(currentIndex); }
        Keys.onDownPressed: { card.keyboardUsed = true; incrementCurrentIndex(); card.picked(currentIndex); }
        onActiveFocusChanged: if (activeFocus && !hoverArea.containsMouse) card.keyboardUsed = true
    }

    Rectangle {
        visible: list.contentHeight > list.height
        anchors.right: parent.right
        anchors.rightMargin: 3
        y: list.y + list.visibleArea.yPosition * list.height
        width: 4
        height: Math.max(24, list.visibleArea.heightRatio * list.height)
        radius: 2
        color: UI.Style.scrollThumb
    }

    // Tells a Tab into the list from a click into it.
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    UI.FocusRing {
        targetRadius: card.radius
        visible: list.activeFocus && card.keyboardUsed
    }
}
