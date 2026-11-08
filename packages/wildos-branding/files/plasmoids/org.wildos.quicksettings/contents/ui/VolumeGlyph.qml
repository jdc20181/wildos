import QtQuick
import WildOS.UI as UI

// The design's speaker. Waves follow the volume, and muted shows a cross.
UI.Glyph {
    property int percent: 50
    property bool muted: false
    // The design draws the slider row's speaker with a single wave.
    property bool oneWave: false

    path: Icons.speaker
        + (muted ? Icons.muteCross
           : (percent > 0 ? Icons.waveNear : "")
             + (!oneWave && percent > 33 ? Icons.waveFar : ""))
}
