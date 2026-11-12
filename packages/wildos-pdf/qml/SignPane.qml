import QtQuick
import QtQuick.Layouts
import WildOS.PDF
import WildOS.UI as UI

// The Sign pane (AppPdf.dc): 300px down the right, your signatures, Draw,
// Type and Image to make one, today's date and initials, and Save a signed
// copy at the foot.
Item {
    id: pane

    required property var app

    signal closeRequested()
    signal drawRequested()
    signal typeRequested()
    signal imageRequested()
    signal saveRequested()

    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: UI.Style.divider
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        anchors.leftMargin: 19
        spacing: 14

        RowLayout {
            Layout.fillWidth: true

            UI.Label {
                Layout.fillWidth: true
                text: qsTr("Sign")
                size: 15
                font.weight: UI.Style.weightSemiBold
            }

            UI.IconButton {
                implicitWidth: 34
                implicitHeight: 34
                iconName: "close"
                iconSize: 15
                tip: qsTr("Close")
                onClicked: pane.closeRequested()
            }
        }

        UI.Label {
            text: qsTr("YOUR SIGNATURES")
            size: 12
            font.weight: UI.Style.weightSemiBold
            font.letterSpacing: 0.24
            color: UI.Style.inkSubtle
        }

        // A signature is a picture of ink on paper, so its tile is paper
        // white in either scheme.
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(70, Math.min(contentHeight, 70 * 3 + 8 * 2))
            visible: count > 0
            clip: true
            spacing: 8
            boundsBehavior: Flickable.StopAtBounds
            model: Signatures.items

            delegate: Rectangle {
                id: tile
                required property var modelData
                readonly property bool chosen: pane.app.signatureId === modelData.id

                width: list.width
                height: 70
                radius: UI.Style.radiusSm
                color: "#ffffff"
                border.width: chosen ? 2 : 1
                border.color: chosen ? UI.Style.accent : UI.Style.borderControl

                Image {
                    anchors.fill: parent
                    anchors.margins: 12
                    source: "image://signature/" + tile.modelData.id + "/" + Signatures.revision
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    cache: false
                }

                MouseArea {
                    id: tileMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: tile.modelData.kind === "type" ? qsTr("Typed signature") : tile.modelData.kind === "image" ? qsTr("Signature from a picture") : qsTr("Drawn signature")
                    onClicked: pane.app.signatureId = tile.modelData.id
                }

                Rectangle {
                    visible: tileMouse.containsMouse || removeButton.hovered
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 6
                    width: 26
                    height: 26
                    radius: 13
                    color: UI.Style.glassMenu
                    border.width: 1
                    border.color: UI.Style.borderMenu

                    UI.IconButton {
                        id: removeButton
                        anchors.fill: parent
                        radius: 13
                        iconName: "close"
                        iconSize: 12
                        tip: qsTr("Remove from your keyring")
                        onClicked: pane.app.askRemoveSignature(tile.modelData.id)
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            visible: list.count === 0
            radius: UI.Style.radiusSm
            color: "transparent"
            border.width: 1
            border.color: UI.Style.borderControl

            UI.Label {
                anchors.fill: parent
                anchors.margins: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: Signatures.ready ? qsTr("No signatures yet. Draw one, type your name or use a picture of it.") : qsTr("Opening your keyring")
                size: 12.5
                color: UI.Style.inkMuted
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            PaneButton {
                Layout.fillWidth: true
                iconName: "draw"
                text: qsTr("Draw")
                onClicked: pane.drawRequested()
            }
            PaneButton {
                Layout.fillWidth: true
                iconName: "text"
                text: qsTr("Type")
                onClicked: pane.typeRequested()
            }
            PaneButton {
                Layout.fillWidth: true
                iconName: "picture"
                text: qsTr("Image")
                onClicked: pane.imageRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            UI.Label {
                Layout.fillWidth: true
                text: qsTr("Add today's date")
            }
            UI.Toggle {
                checked: pane.app.addDate
                onToggled: pane.app.addDate = checked
                Accessible.name: qsTr("Add today's date")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            UI.Label {
                Layout.fillWidth: true
                text: qsTr("Add my initials to each page")
            }
            UI.Toggle {
                checked: pane.app.addInitials
                onToggled: pane.app.setAddInitials(checked)
                Accessible.name: qsTr("Add my initials to each page")
            }
        }

        UI.TextField {
            id: initialsField
            Layout.fillWidth: true
            visible: pane.app.addInitials
            placeholderText: qsTr("Your initials")
            text: Signatures.initials
            maximumLength: 5
            onEditingFinished: Signatures.setInitials(text)
            Accessible.name: qsTr("Your initials")
        }

        UI.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            size: 12.5
            lineHeight: 1.5
            color: UI.Style.inkMuted
            text: Signatures.kept ? qsTr("Click where the signature goes, then drag the corners to size it. Signatures are kept on this PC, in your keyring.")
                : qsTr("Click where the signature goes, then drag the corners to size it. Your keyring couldn't be opened, so signatures you add are kept only until PDF closes.")
        }

        Item {
            Layout.fillHeight: true
        }

        PaneButton {
            Layout.fillWidth: true
            primary: true
            iconName: "save"
            text: qsTr("Save a signed copy")
            enabled: pane.app.hasSignaturePlaced
            onClicked: pane.saveRequested()
        }

        UI.Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            size: 12
            color: UI.Style.inkSubtle
            text: qsTr("The original stays as it is.")
        }
    }
}
