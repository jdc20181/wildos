import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Templates as T

// One choice on the recovery screens: an icon, a 15px title with an
// optional badge, a line under it, and a chevron when it opens more. The
// chosen row (keyboard or mouse) is outlined in the accent, as the boards
// draw "Start the earlier version".
T.AbstractButton {
    id: row

    property string iconPath
    property string badge
    property string detail
    property bool chevron: false
    property bool selected: activeFocus
    property int rowHeight: 70
    property real scale_: 1

    implicitHeight: Math.max(rowHeight * scale_, content.implicitHeight + 20)
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.description: detail
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    background: Rectangle {
        radius: 12
        color: row.selected ? Qt.rgba(143 / 255, 180 / 255, 1, 0.12) : row.hovered ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(1, 1, 1, 0.05)
        border.width: row.selected ? 2 : 1
        border.color: row.selected ? "#8fb4ff" : Qt.rgba(1, 1, 1, 0.10)
    }

    contentItem: RowLayout {
        id: content
        spacing: 16

        Shape {
            Layout.leftMargin: 18 - (row.selected ? 1 : 0)
            Layout.preferredWidth: 22 * row.scale_
            Layout.preferredHeight: 22 * row.scale_
            scale: row.scale_ * 22 / 24
            transformOrigin: Item.TopLeft
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: row.selected && row.badge.length > 0 ? "#8fb4ff" : "#ffffff"
                strokeWidth: 1.8
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: row.iconPath }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            RowLayout {
                spacing: 10

                Text {
                    text: row.text
                    color: "#ffffff"
                    font.family: "Albert Sans"
                    font.pixelSize: 15 * row.scale_
                    font.weight: 600
                }

                Rectangle {
                    visible: row.badge.length > 0
                    implicitWidth: badgeText.implicitWidth + 16
                    implicitHeight: 20 * row.scale_
                    radius: height / 2
                    color: Qt.rgba(143 / 255, 180 / 255, 1, 0.14)

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: row.badge
                        color: "#8fb4ff"
                        font.family: "Albert Sans"
                        font.pixelSize: 11.5 * row.scale_
                        font.weight: 600
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: row.detail.length > 0
                text: row.detail
                color: Qt.rgba(1, 1, 1, 0.72)
                font.family: "Albert Sans"
                font.pixelSize: 12.5 * row.scale_
                wrapMode: Text.Wrap
            }
        }

        Shape {
            visible: row.chevron
            Layout.rightMargin: 18
            Layout.preferredWidth: 18
            Layout.preferredHeight: 18
            scale: 18 / 24
            transformOrigin: Item.TopLeft
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: Qt.rgba(1, 1, 1, 0.6)
                strokeWidth: 1.8
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: "m9 18 6-6-6-6" }
            }
        }

        Item {
            visible: !row.chevron
            Layout.preferredWidth: 2
        }
    }
}
