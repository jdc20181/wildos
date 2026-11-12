// Puts the Updates icon on a taskbar made before it existed, just after the
// security chip. New accounts get it from the Wild OS layout, and a taskbar
// that has it already, or has no security chip to sit beside, is left alone.
// taskbar-items runs this once per account, on a running shell.
panels().forEach(function (p) {
    var chips = p.widgets("org.wildos.protected");
    if (chips.length === 0 || p.widgets("org.wildos.updates").length > 0) {
        return;
    }
    // The layout locks the bar, and Plasma adds nothing to a locked one.
    var locked = p.locked;
    p.locked = false;
    // Plasma's scripting can't say where in a panel a widget goes, but a
    // panel puts one dropped on the right half of another just after it.
    // Three quarters along, because the bar's own coordinates sit a few
    // pixels off the ones a drop is measured in, and the right edge landed
    // past the divider beside the chip.
    var g = chips[0].geometry;
    // The size has to be given too: without one the position is ignored
    // and the widget lands at the start of the bar.
    var item = p.addWidget("org.wildos.updates", g.x + g.width * 0.75, g.y + g.height / 2, 44, g.height);
    if (item) {
        // Locked like the rest of the bar (see the layout), so it can't be
        // removed or moved by accident.
        item.currentConfigGroup = [];
        item.writeConfig("immutability", 2);
        item.locked = true;
    }
    p.locked = locked;
});
