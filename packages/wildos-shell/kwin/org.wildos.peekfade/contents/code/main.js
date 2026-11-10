// While the desktop shows (the taskbar's peek), windows fade out where they
// stand and fade back in place afterwards, as on Windows. KWin's own Window
// Aperture slides them off to the screen edges instead, which looks like
// they are going somewhere. Clicking the taskbar corner and Windows key + D
// minimise windows instead (org.wildos.showdesktop).

"use strict";

const duration = animationTime(180);

function peeked(w) {
    return (w.normalWindow || w.dialog) && !w.desktopWindow && !w.dock;
}

function fade(w, opacity) {
    // A peek ended before its fade finished: the new fade replaces it.
    if (w.wildosPeekFade !== undefined) {
        cancel(w.wildosPeekFade);
    }
    opacity.type = Effect.Opacity;
    opacity.curve = QEasingCurve.OutCubic;
    w.wildosPeekFade = animate({ window: w, duration: duration, animations: [opacity] });
}

effects.showingDesktopChanged.connect(showing => {
    for (const w of effects.stackingOrder) {
        if (!peeked(w)) {
            continue;
        }
        if (showing) {
            if (w.visible) {
                w.wildosPeeked = true;
                fade(w, { to: 0.0 });
            }
        } else if (w.wildosPeeked) {
            w.wildosPeeked = false;
            // Minimised during the peek, so it went to the taskbar and stays
            // there.
            if (!w.minimized) {
                fade(w, { from: 0.0 });
            }
        }
    }
});
