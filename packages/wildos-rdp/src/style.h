#pragma once

#include <QColor>

// The few WildOS.UI tokens the loading window needs, read the way
// wildos-ui's Theme reads them (kdeglobals, wildosrc and the theme's JSON),
// so the window matches the rest of the desktop without loading QML.
struct Style {
    bool dark = true;
    bool contrast = false;
    bool reducedMotion = false;
    // Style.glassWindow, Style.borderGlass and Style.ink.
    QColor glass;
    QColor border;
    QColor ink;
    QColor accent;

    static Style read();
};
