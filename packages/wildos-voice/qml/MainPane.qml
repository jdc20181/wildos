import QtQuick
import QtQuick.Layouts
import WildOS.Voice
import WildOS.UI as UI

// The right of the window (AppVoice.dc). Three states share one layout: the
// title row, the big time, the waveform with its marker flags, the round
// buttons and the Transcript card.
//   idle    nothing picked: a big Record button where Stop would be
//   record  the recording in progress: Mark, Stop and Pause
//   play    a finished recording: back, play and forward, its markers to
//           jump to, and its transcript
Item {
    id: pane

    property string mode: "idle"
    // The recording shown in "play".
    property var recording: ({})
    property var bars: []

    signal renameRequested()
    signal deleteRequested()
    signal newRecordingRequested()

    readonly property double shownTime: mode === "record" ? Recorder.elapsed : mode === "play" ? Player.position : 0
    readonly property double length: mode === "play" ? Math.max(Player.duration, recording.duration ?? 0) : 0
    readonly property var markers: mode === "record" ? Recorder.markers : mode === "play" ? (recording.markers ?? []) : []

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 40
        anchors.rightMargin: 40
        anchors.topMargin: 22
        anchors.bottomMargin: 26
        spacing: 22

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            spacing: 12

            Rectangle {
                visible: pane.mode === "record"
                Layout.leftMargin: 4
                width: 10
                height: 10
                radius: 5
                color: VoiceStyle.record
                opacity: Recorder.state === "paused" ? 0.5 : 1

                Rectangle {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    radius: 9
                    color: "transparent"
                    border.width: 4
                    border.color: VoiceStyle.recordRing
                }
            }
            UI.Label {
                Layout.maximumWidth: pane.width * 0.45
                text: pane.mode === "record" ? Recorder.title : pane.mode === "play" ? (pane.recording.title ?? "") : qsTr("New recording")
                size: 15
                font.weight: UI.Style.weightSemiBold
            }
            UI.Label {
                visible: text.length > 0
                text: pane.mode === "record" && Recorder.state === "paused" ? qsTr("Paused")
                    : pane.mode === "play" && pane.recording.created ? VoiceStyle.fullDate(pane.recording.created) : ""
                size: 12.5
                color: UI.Style.inkMuted
            }
            Item {
                Layout.fillWidth: true
            }

            MicPicker {
                visible: pane.mode !== "play"
            }

            Row {
                visible: pane.mode === "play"
                spacing: 2

                ToolButton {
                    iconName: "rename"
                    tip: qsTr("Rename")
                    onClicked: pane.renameRequested()
                }
                ToolButton {
                    iconName: "open-folder"
                    tip: qsTr("Show in Files")
                    onClicked: Recordings.showInFiles(pane.recording.path)
                }
                ToolButton {
                    iconName: "delete"
                    tip: qsTr("Delete")
                    onClicked: pane.deleteRequested()
                }
                Item {
                    width: 10
                    height: 1
                }
                UI.Button {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("New recording")
                    iconName: "mic"
                    size: "small"
                    onClicked: pane.newRecordingRequested()
                }
            }
        }

        UI.Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: -4
            text: VoiceStyle.time(pane.shownTime)
            size: 56
            font.weight: UI.Style.weightLight
            font.features: { "tnum": 1 }
            font.letterSpacing: UI.Style.px(56) * 0.02
        }

        Item {
            id: waveArea
            Layout.fillWidth: true
            Layout.preferredHeight: 170

            WaveformView {
                id: wave
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width, 763)
                height: parent.height
                live: pane.mode !== "play"
                bars: pane.mode === "record" ? Recorder.recentBars : pane.mode === "play" ? pane.bars : []
                progress: pane.length > 0 ? pane.shownTime / pane.length : 0
                barColor: pane.mode === "play" ? VoiceStyle.waveRest : VoiceStyle.record
                playedColor: UI.Style.accent
                dotColor: VoiceStyle.waveDot
                playheadColor: VoiceStyle.playhead
                showPlayhead: pane.mode !== "idle"

                MouseArea {
                    enabled: pane.mode === "play" && pane.length > 0
                    anchors.fill: parent
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onPressed: mouse => Player.seek(wave.fractionAt(mouse.x) * pane.length)
                    onPositionChanged: mouse => {
                        if (pressed) {
                            Player.seek(wave.fractionAt(mouse.x) * pane.length);
                        }
                    }
                }

                // A flag above the waveform at each marker; in a finished
                // recording, a click jumps there.
                Repeater {
                    model: pane.markers

                    delegate: Item {
                        id: flag
                        required property var modelData
                        readonly property double at: pane.mode === "record" ? wave.liveX(Recorder.elapsed - modelData)
                            : pane.length > 0 ? wave.fullX(modelData / pane.length) : -1
                        visible: at >= 0
                        x: at - 7
                        y: -6
                        width: 14
                        height: 14

                        VoiceIcon {
                            anchors.fill: parent
                            name: "flag"
                            size: 14
                            color: UI.Style.accent
                        }

                        MouseArea {
                            enabled: pane.mode === "play"
                            anchors.fill: parent
                            anchors.margins: -6
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: Player.seek(flag.modelData)
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: controls.implicitHeight
            Layout.topMargin: -8

            Row {
                id: controls
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 22

                RoundButton {
                    visible: pane.mode === "idle"
                    diameter: 72
                    glyph: "record"
                    fill: VoiceStyle.record
                    fillHover: Qt.lighter(VoiceStyle.record, 1.08)
                    halo: VoiceStyle.recordHalo
                    label: qsTr("Record")
                    enabled: Recorder.hasMicrophone
                    onClicked: Recorder.start()
                }

                RoundButton {
                    visible: pane.mode === "record"
                    iconName: "flag"
                    label: qsTr("Mark")
                    onClicked: Recorder.mark()
                }
                RoundButton {
                    visible: pane.mode === "record"
                    diameter: 72
                    glyph: "stop"
                    fill: VoiceStyle.record
                    fillHover: Qt.lighter(VoiceStyle.record, 1.08)
                    halo: VoiceStyle.recordHalo
                    label: qsTr("Stop")
                    onClicked: Recorder.stop()
                }
                RoundButton {
                    visible: pane.mode === "record"
                    iconName: Recorder.state === "paused" ? "mic" : "pause"
                    label: Recorder.state === "paused" ? qsTr("Resume") : qsTr("Pause")
                    onClicked: Recorder.state === "paused" ? Recorder.resume() : Recorder.pause()
                }

                RoundButton {
                    visible: pane.mode === "play"
                    iconName: "back"
                    label: qsTr("Back 10 s")
                    onClicked: Player.skip(-10000)
                }
                RoundButton {
                    visible: pane.mode === "play"
                    diameter: 72
                    iconName: Player.playing ? "pause" : "play"
                    iconSize: 26
                    iconColor: UI.Style.textOnAccent
                    fill: UI.Style.accent
                    fillHover: UI.Style.accentHover
                    label: Player.playing ? qsTr("Pause") : qsTr("Play")
                    onClicked: Player.playPause()
                }
                RoundButton {
                    visible: pane.mode === "play"
                    iconName: "forward"
                    label: qsTr("Forward 10 s")
                    onClicked: Player.skip(10000)
                }
            }
        }

        UI.Label {
            visible: text.length > 0
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: -10
            horizontalAlignment: Text.AlignHCenter
            text: pane.mode === "idle" && !Recorder.hasMicrophone ? qsTr("No microphone found. Plug one in, or check Settings > Sound.")
                : pane.mode === "record" && Recorder.silent && Recorder.state === "recording" ? qsTr("Voice can't hear anything. Check that the microphone isn't muted in Settings > Sound.")
                : ""
            size: 13
            color: UI.Style.warning
        }

        // The markers of a finished recording, to jump to.
        Flow {
            visible: pane.mode === "play" && pane.markers.length > 0
            Layout.fillWidth: true
            Layout.topMargin: -6
            spacing: 8

            Repeater {
                model: pane.mode === "play" ? pane.markers : []

                delegate: MarkerChip {
                    required property var modelData
                    required property int index
                    time: modelData
                    text: qsTr("Marker %1, %2").arg(index + 1).arg(VoiceStyle.time(modelData))
                    onClicked: Player.seek(modelData)
                }
            }
        }

        TranscriptCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumHeight: implicitHeight
            mode: pane.mode
            path: pane.mode === "record" ? Recorder.path : pane.mode === "play" ? (pane.recording.path ?? "") : ""
            canSeek: pane.mode === "play"
            onSeek: ms => Player.seek(ms)
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
