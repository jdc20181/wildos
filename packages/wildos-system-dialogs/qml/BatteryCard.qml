import QtQuick
import WildOS.UI as UI

// "Battery low: 10%" (SystemPrompts.dc, top middle). Battery saver is the
// power-saver profile; its button is only there when the computer has one
// and it is not already on.
PromptCard {
    id: card

    property int percent: 10
    property int minutes: 0
    property bool saver: false
    property bool saverOn: false

    signal dismissed()
    signal saverChosen()

    readonly property bool offerSaver: saver && !saverOn

    function timeLeft(m) {
        if (m <= 0) {
            return "";
        }
        if (m < 60) {
            return "About " + m + (m === 1 ? " minute" : " minutes") + " left. ";
        }
        const h = Math.floor(m / 60);
        const rest = m % 60;
        return "About " + h + (h === 1 ? " hour" : " hours") + (rest > 0 ? " " + rest + (rest === 1 ? " minute" : " minutes") : "") + " left. ";
    }

    width: 420
    title: "Battery low: " + percent + "%"
    text: timeLeft(minutes) + (offerSaver ? "Plug in soon, or turn on battery saver to make it last longer."
                          : saverOn ? "Plug in soon. Battery saver is already on."
                          : "Plug in soon.")

    picture: BatteryPicture {}

    buttons: [
        UI.Button {
            id: dismissButton
            kind: card.offerSaver ? "secondary" : "primary"
            text: "Dismiss"
            onClicked: card.dismissed()
        },
        UI.Button {
            id: saverButton
            visible: card.offerSaver
            kind: "primary"
            text: "Turn on battery saver"
            onClicked: card.saverChosen()
        }
    ]

    Keys.onEscapePressed: card.dismissed()
    function takeFocus() {
        (offerSaver ? saverButton : dismissButton).forceActiveFocus();
    }
}
