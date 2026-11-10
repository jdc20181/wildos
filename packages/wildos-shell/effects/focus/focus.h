/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <effect/effectwindow.h>

// Shared by the outline and background blur effects (Settings >
// Accessibility), so both agree on which window is the one in use.
namespace KWin::WildOS
{

// A window someone works in. Panels, the desktop, menus, tooltips,
// notifications, on-screen displays, Plasma's pop-ups (Start and the
// flyouts), the on-screen keyboard, the lock screen and KWin's own windows
// are none of these.
inline bool isWorkWindow(EffectWindow *w)
{
    // KWin's own windows (the taskbar underglow script draws in one) are
    // part of the desktop, however they are typed.
    if (!w || !w->isManaged() || w->internalWindow()) {
        return false;
    }
    if (w->isSpecialWindow() || w->isPopupWindow() || w->isAppletPopup() || w->isInputMethod() || w->isLockScreen() || w->isOutline()) {
        return false;
    }
    return w->isNormalWindow() || w->isDialog() || w->isUtility();
}

// The window in use after another is activated. Start, a flyout, a menu or
// the taskbar taking focus leaves it where it was, because keyboard focus
// goes back there when they close; so does focus going nowhere for a
// moment, which is how clicking the taskbar to open Start begins. Clicking
// the desktop, or the window in use going away, means no window is in use.
inline EffectWindow *focusAfterActivation(EffectWindow *activated, EffectWindow *current)
{
    if (activated && activated->isDesktop()) {
        return nullptr;
    }
    if (activated && isWorkWindow(activated)) {
        return activated;
    }
    if (current && (current->isDeleted() || current->isMinimized())) {
        return nullptr;
    }
    return current;
}

// A dialog and the window it belongs to are one piece of work: the window
// behind a Save dialog is still the one being used.
inline bool belongTogether(EffectWindow *a, EffectWindow *b)
{
    if (!a || !b) {
        return false;
    }
    if (a == b) {
        return true;
    }
    return a->mainWindows().contains(b) || b->mainWindows().contains(a);
}

}
