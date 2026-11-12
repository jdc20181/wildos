pragma Singleton

import QtQuick
import WildOS.UI as UI

// The values the board gives Videos that the system tokens do not have.
// The picture and its controls stay dark in both schemes (AppVideos.dc and
// AppVideosLight.dc draw them the same), so their colours are fixed here;
// only the side panel and the title bar follow the scheme.
QtObject {
    // The title bar sits on a deeper shade of the window than the panel
    // (#0e0f12 against #181a20; #eeeeee against #f4f4f4).
    readonly property color titleShade: UI.Style.contrast ? "#000000" : Qt.rgba(0, 0, 0, UI.Style.dark ? 0.42 : 0.025)
    readonly property color rowHover: UI.Style.contrast ? UI.Style.surfaceHover : UI.Style.dark ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.049)
    readonly property color rowSelected: UI.Style.contrast ? UI.Style.surfaceSelected : UI.Style.dark ? Qt.rgba(1, 1, 1, 0.061) : Qt.rgba(0, 0, 0, 0.043)

    readonly property color picture: "#000000"
    readonly property color overPicture: "#ffffff"
    readonly property color overPictureMuted: Qt.rgba(1, 1, 1, UI.Style.contrast ? 1 : 0.8)
    readonly property color overPictureFaint: Qt.rgba(1, 1, 1, UI.Style.contrast ? 1 : 0.6)
    readonly property color overPictureIcon: Qt.rgba(1, 1, 1, UI.Style.contrast ? 1 : 0.86)
    readonly property color overPictureHover: Qt.rgba(1, 1, 1, 0.07)
    readonly property color overPicturePressed: Qt.rgba(1, 1, 1, 0.12)
    readonly property color overPictureChip: Qt.rgba(1, 1, 1, 0.12)
    readonly property color overPictureTrack: Qt.rgba(1, 1, 1, 0.18)
    // The seek bar takes the accent as the dark scheme draws it, since the
    // picture is dark in both (#8fb4ff on the light board too).
    readonly property color overPictureAccent: UI.Style.contrast ? UI.Style.accent : UI.Style.systemAccent
    readonly property color chapterTick: Qt.rgba(0, 0, 0, 0.6)
    readonly property color tipGlass: Qt.rgba(20 / 255, 22 / 255, 28 / 255, 0.92)
    readonly property color tipEdge: Qt.rgba(1, 1, 1, 0.12)
    readonly property color durationChip: Qt.rgba(0, 0, 0, 0.7)

    // "12:48", "1:02:03".
    function time(seconds) {
        if (!(seconds > 0)) {
            return "0:00";
        }
        const total = Math.floor(seconds);
        const h = Math.floor(total / 3600);
        const m = Math.floor((total % 3600) / 60);
        const s = total % 60;
        const ss = (s < 10 ? "0" : "") + s;
        return h > 0 ? h + ":" + (m < 10 ? "0" : "") + m + ":" + ss : m + ":" + ss;
    }
    // The thumbnails' lengths are written with two figures for minutes, as
    // the board does: "08:40", "24:12", "1:02:03".
    function length(seconds) {
        if (!(seconds > 0)) {
            return "";
        }
        const total = Math.round(seconds);
        const h = Math.floor(total / 3600);
        const m = Math.floor((total % 3600) / 60);
        const s = total % 60;
        const two = n => (n < 10 ? "0" : "") + n;
        return h > 0 ? h + ":" + two(m) + ":" + two(s) : two(m) + ":" + two(s);
    }
    function speed(value) {
        return Number(value).toLocaleString(Qt.locale(), "f", value % 1 === 0 ? 0 : value % 0.5 === 0 ? 1 : 2) + "×";
    }
    function count(n) {
        return n === 1 ? qsTr("1 video") : qsTr("%1 videos").arg(Number(n).toLocaleString(Qt.locale(), "f", 0));
    }
}
