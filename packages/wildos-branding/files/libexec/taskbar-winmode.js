// Puts Windows mode's status on a taskbar made before it existed (boards
// 4.13 and 4.14), at the left end of the bar in either style. New accounts get it from the Wild OS layout, and a
// taskbar that has it already is left alone. taskbar-items runs this once
// per account, on a running shell: from a Plasma update script, which runs
// while the shell is starting, unlocking the bar left it undrawn until the
// next sign-in.
panels().forEach(function (p) {
    if (p.widgets("org.wildos.tasks").length === 0 || p.widgets("org.wildos.winmode").length > 0) {
        return;
    }
    // The layout locks the bar, and Plasma adds nothing to a locked one.
    var locked = p.locked;
    p.locked = false;
    // Dropped on the bar's first pixel, a panel puts it before everything.
    var item = p.addWidget("org.wildos.winmode", 0, 0, -1, -1);
    if (item) {
        // Locked like the rest of the bar (see the layout), so it can't be
        // removed or moved by accident.
        item.currentConfigGroup = [];
        item.writeConfig("immutability", 2);
        item.locked = true;
    }
    p.locked = locked;
});
