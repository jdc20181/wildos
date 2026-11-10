import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// "Restart to finish updating" (SystemPrompts.dc, top right). Wild OS never
// restarts on its own: the person restarts now, or picks a time from the
// list and confirms it with Schedule, or says not now and is asked again
// later. At a time they picked, a minute's warning comes first and Not now
// still stops it.
PromptCard {
    id: card

    property bool security: false
    // "3 fixes and 2 improvements", when Settings knows what the update brings.
    property string summary
    property bool countdown: false
    property int seconds: 0
    property string scheduled
    property var times: []
    property bool picked: false

    signal restartNow()
    signal scheduleAt(string when)
    signal notNow()

    width: 400
    title: countdown ? "Restarting to finish updating" : "Restart to finish updating"
    text: countdown
        ? "Wild OS will restart in " + seconds + (seconds === 1 ? " second" : " seconds") + ", at the time you chose. Your apps and windows will reopen where you left them."
        : (security ? "A security update is ready" : "An update is ready") + (summary.length > 0 ? ", with " + summary + "." : ".")
            + " Your apps and windows will reopen where you left them."

    picture: Rectangle {
        width: 40
        height: 40
        radius: 20
        color: UI.Style.accentAlpha(0.16, 0.12)

        UI.Icon {
            anchors.centerIn: parent
            name: "restart"
            size: 20
            color: UI.Style.accentText
        }
    }

    extra: [
        UI.Label {
            Layout.fillWidth: true
            visible: !card.countdown
            text: card.scheduled.length > 0 && !card.picked
                ? "Restarting " + card.scheduled.charAt(0).toLowerCase() + card.scheduled.slice(1) + ". Or pick another time:"
                : "Or restart automatically:"
            size: 12
            color: UI.Style.inkStatus
            wrapMode: Text.Wrap
            elide: Text.ElideNone
        },
        UI.ComboBox {
            id: when
            Layout.fillWidth: true
            implicitHeight: 32
            visible: !card.countdown && card.times.length > 0
            model: card.times
            Accessible.name: "Restart automatically"
            onActivated: card.picked = true
        }
    ]

    buttons: [
        UI.Button {
            text: card.picked ? "Schedule" : "Not now"
            onClicked: card.picked ? card.scheduleAt(when.currentValue) : card.notNow()
        },
        UI.Button {
            id: nowButton
            kind: "primary"
            text: "Restart now"
            onClicked: card.restartNow()
        }
    ]

    onTimesChanged: picked = false
    Keys.onEscapePressed: card.notNow()
    function takeFocus() {
        nowButton.forceActiveFocus();
    }
}
