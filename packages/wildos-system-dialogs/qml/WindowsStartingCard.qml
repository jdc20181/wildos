import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// A Windows app was opened while Windows is off or asleep. Windows mode
// shows this until the app's window is up, so a start that takes a minute
// never looks like nothing happened. The bar follows Windows mode's own
// estimate, slowing as it nears the end rather than stopping, and the card
// never takes the keyboard: it is only there to watch.
PromptCard {
    id: card

    property string appName
    property string iconName
    property int seconds: 60
    property bool cold: false

    signal hidden()

    width: 440
    title: cold ? "Starting Windows" : "Waking up Windows"
    text: (appName.length > 0 ? appName : "Your app") + " opens " + when() + "."

    function when() {
        if (seconds <= 5)
            return "in a few seconds";
        if (seconds < 45)
            return "in about " + (Math.round(seconds / 5) * 5) + " seconds";
        if (seconds < 90)
            return "as soon as Windows is ready, in about a minute";
        return "as soon as Windows is ready, in about " + Math.round(seconds / 60) + " minutes";
    }

    property real elapsed: 0
    readonly property real progress: 0.95 * (1 - Math.exp(-2.2 * elapsed / Math.max(1, seconds)))

    Timer {
        interval: 200
        running: true
        repeat: true
        triggeredOnStart: true
        property double startedAt: Date.now()
        onTriggered: card.elapsed = (Date.now() - startedAt) / 1000
    }

    picture: AppPicture {
        iconName: card.iconName.length > 0 ? card.iconName : "wildos-windows-app"
    }

    extra: [
        UI.ProgressBar {
            Layout.fillWidth: true
            Layout.topMargin: 6
            from: 0
            to: 1
            value: card.progress
            Accessible.name: card.title
        }
    ]

    buttons: [
        UI.Button {
            text: "Hide"
            onClicked: card.hidden()
        }
    ]

    Keys.onEscapePressed: card.hidden()
    function takeFocus() {
    }
}
