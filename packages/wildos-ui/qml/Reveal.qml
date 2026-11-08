import QtQuick

// The arrival of a page, a view or a pop-up: it fades in while sliding the
// last few pixels into place. Set it as the item's transform so the slide
// stays on the GPU and never moves the layout:
//
//     Item { id: page; transform: UI.Reveal { item: page; dx: UI.Style.travelSmall } }
//
// It plays whenever the item becomes visible, and whenever `key` changes
// (for a Loader whose page is swapped while the Loader stays visible). It
// owns the item's opacity, so the item must not bind opacity itself.
// Nothing plays while the window is hidden, so a pop-up that is reset as
// it closes is already in place when it next opens.
Translate {
    id: reveal

    property Item item
    // Where the item starts, relative to where it ends up.
    property real dx: 0
    property real dy: 0
    property int duration: Style.motionNormal
    property var key
    // With a numeric key (a step or a month), going back to a lower key
    // arrives from the opposite side, so back feels like back.
    property bool directional: false
    property bool enabled: true

    property var _lastKey

    function play(reverse: bool): void {
        if (!item) {
            return;
        }
        animation.stop();
        const window = item.Window.window;
        if (!enabled || duration <= 0 || !item.visible || !window || !window.visible) {
            settle();
            return;
        }
        x = reverse ? -dx : dx;
        y = reverse ? -dy : dy;
        item.opacity = 0;
        animation.start();
    }

    function settle(): void {
        animation.stop();
        x = 0;
        y = 0;
        if (item) {
            item.opacity = 1;
        }
    }

    onKeyChanged: {
        const back = directional && typeof key === "number" && typeof _lastKey === "number" && key < _lastKey;
        _lastKey = key;
        play(back);
    }
    Component.onCompleted: _lastKey = key

    readonly property Connections watcher: Connections {
        target: reveal.item
        function onVisibleChanged() {
            if (reveal.item.visible) {
                reveal.play(false);
            } else {
                reveal.settle();
            }
        }
    }

    readonly property ParallelAnimation animation: ParallelAnimation {
        NumberAnimation {
            target: reveal
            properties: "x,y"
            to: 0
            duration: reveal.duration
            easing.type: Style.easeEnter
        }
        NumberAnimation {
            target: reveal.item
            property: "opacity"
            to: 1
            // Opacity settles before the slide does, so the page is
            // readable while it finishes moving.
            duration: Math.round(reveal.duration * 0.75)
            easing.type: Easing.OutCubic
        }
    }
}
