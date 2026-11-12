import QtQuick
import QtQuick.Layouts
import WildOS.Archive
import WildOS.UI as UI

// The pane at the right (AppArchive.dc): what will be extracted, up to
// three of the pictures in it, where to, the options, the size and a rough
// time, and Extract. While a job runs its progress and Cancel take the
// place of the button.
Rectangle {
    id: pane

    required property Archive archive

    signal chooseFolder()

    color: ArchiveStyle.paneShade

    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: ArchiveStyle.hairline
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        anchors.leftMargin: 19
        spacing: 14

        UI.Label {
            Layout.fillWidth: true
            text: pane.archive.paneTitle
            size: 15
            font.weight: UI.Style.weightSemiBold
            elide: Text.ElideRight
        }

        Row {
            visible: pane.archive.paneThumbs.length > 0
            spacing: 6

            Repeater {
                model: pane.archive.paneThumbs

                Rectangle {
                    required property string modelData
                    width: 88
                    height: 66
                    radius: 6
                    color: ArchiveStyle.thumbBlank
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: parent.modelData
                        sourceSize.width: 176
                        sourceSize.height: 132
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        layer.enabled: true
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            UI.Label {
                text: qsTr("To")
                size: 12.5
                color: UI.Style.inkMuted
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    radius: 6
                    color: ArchiveStyle.fieldFill
                    border.width: 1
                    border.color: ArchiveStyle.fieldEdge

                    UI.Label {
                        anchors.fill: parent
                        anchors.leftMargin: 11
                        anchors.rightMargin: 10
                        verticalAlignment: Text.AlignVCenter
                        text: pane.archive.destinationText
                        size: 13
                        elide: Text.ElideLeft
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        enabled: !pane.archive.busy
                        onClicked: pane.chooseFolder()
                    }
                }
                ToolButton {
                    side: 34
                    iconName: "folder-open"
                    tip: qsTr("Choose folder")
                    enabled: !pane.archive.busy
                    onClicked: pane.chooseFolder()
                }
            }
        }

        CheckMark {
            text: qsTr("Keep the folder structure")
            checked: pane.archive.keepStructure
            enabled: !pane.archive.busy
            onToggled: pane.archive.keepStructure = checked
        }
        CheckMark {
            Layout.topMargin: -4
            text: qsTr("Open the folder when done")
            checked: pane.archive.openWhenDone
            enabled: !pane.archive.busy
            onToggled: pane.archive.openWhenDone = checked
        }
        CheckMark {
            Layout.topMargin: -4
            text: qsTr("Replace files with the same name")
            checked: pane.archive.replaceExisting
            enabled: !pane.archive.busy
            onToggled: pane.archive.replaceExisting = checked
        }

        UI.Label {
            Layout.fillWidth: true
            text: pane.archive.paneSize
            size: 12.5
            color: UI.Style.inkMuted
            elide: Text.ElideRight
        }

        Item {
            Layout.fillHeight: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: pane.archive.job === "extract" || pane.archive.job === "test"
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                UI.Label {
                    Layout.fillWidth: true
                    text: pane.archive.jobText
                    size: 12.5
                    elide: Text.ElideMiddle
                }
                UI.Label {
                    text: Math.round(pane.archive.jobProgress * 100) + "%"
                    size: 12.5
                    color: UI.Style.inkMuted
                }
            }
            UI.ProgressBar {
                Layout.fillWidth: true
                value: pane.archive.jobProgress
            }
            UI.Button {
                Layout.fillWidth: true
                size: "small"
                text: qsTr("Cancel")
                onClicked: pane.archive.cancel()
            }
        }

        ToolButton {
            Layout.fillWidth: true
            visible: !(pane.archive.job === "extract" || pane.archive.job === "test")
            accent: true
            iconName: "extract-all"
            text: qsTr("Extract")
            enabled: pane.archive.state === "ready" && !pane.archive.busy
            onClicked: pane.archive.extract(false)
        }
    }
}
