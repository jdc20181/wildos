// Moves Windows mode's status to the left end of a floating bar, where it
// went just before Start until 2026-11-09. Same steps as Settings' own
// placement (wildos-settings system.cpp): off, then back on the bar's
// first pixel, which a panel takes to mean "before everything".
panels().forEach(function (p) {
    var all = p.widgets();
    all.sort(function (a, b) { return a.geometry.x - b.geometry.x; });
    var w = -1;
    for (var i = 0; i < all.length; i++) {
        if (all[i].type === "org.wildos.winmode") {
            w = i;
        }
    }
    if (w <= 0) {
        return;
    }
    var locked = p.locked;
    p.locked = false;
    all[w].remove();
    var item = p.addWidget("org.wildos.winmode", 0, 0, -1, -1);
    if (item) {
        item.currentConfigGroup = [];
        item.writeConfig("immutability", 2);
        item.locked = true;
    }
    p.locked = locked;
});
