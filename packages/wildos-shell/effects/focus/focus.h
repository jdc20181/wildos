/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <effect/effectwindow.h>
#include <window.h>

#include <QList>

// Shared by the outline and background blur effects (Settings >
// Accessibility), so both agree on which window is the one in use.
namespace KWin::WildOS
{

// Set by the background blur on each window it blurs, to how far blurred
// it is (0 to 1), so the outline can fade out with it. KWin keeps roles
// under 1000 for itself.
inline constexpr int BlurProgressRole = 7713;

// A window someone works in. Panels, the desktop, menus, tooltips,
// notifications, on-screen displays, Plasma's pop-ups (Start and the
// flyouts), the on-screen keyboard, the lock screen, overlays such as the
// screenshot tools' and KWin's own windows are none of these.
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
    // Layer shell surfaces are panels and overlays, such as the screenshot
    // overlay, its countdown and the recording bar. KWin types one whose
    // scope it does not know as a normal window, which made the screenshot
    // overlay the window in use and blurred everything under it, into the
    // picture too.
    if (w->window() && w->window()->inherits("KWin::LayerShellV1Window")) {
        return false;
    }
    // Spectacle is a camera, not work. Its area picker is an ordinary window
    // covering the screen, and its main window hides itself to take the
    // picture: as the window in use it blurred everything else, and the
    // capture caught those windows before they had sharpened again.
    if (w->windowClass().contains(QLatin1String("spectacle"), Qt::CaseInsensitive)) {
        return false;
    }
    return w->isNormalWindow() || w->isDialog() || w->isUtility();
}

// The window in use after another is activated. Start, a flyout, a menu,
// a notification, an overlay or the taskbar taking focus leaves it where it
// was, because keyboard focus goes back there when they close; so does
// focus going nowhere for a moment, which is how clicking the taskbar to
// open Start begins. Clicking the desktop, or the window in use going away,
// means no window is in use.
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

// When the window in use closes, the window it belongs to is in use
// again: KWin hands focus back to it, but not always before the next frame.
inline EffectWindow *focusAfterClosing(EffectWindow *closed)
{
    const auto mains = closed->mainWindows();
    for (EffectWindow *main : mains) {
        if (main && !main->isDeleted() && !main->isMinimized() && isWorkWindow(main)) {
            return main;
        }
    }
    return nullptr;
}

// The window and every window it belongs to, up through dialogs opened
// from dialogs.
inline QList<EffectWindow *> withMainWindows(EffectWindow *w)
{
    QList<EffectWindow *> chain{w};
    for (qsizetype i = 0; i < chain.size() && chain.size() < 32; ++i) {
        const auto mains = chain[i]->mainWindows();
        for (EffectWindow *main : mains) {
            if (main && !chain.contains(main)) {
                chain.append(main);
            }
        }
    }
    return chain;
}

inline bool isDialogLike(EffectWindow *w)
{
    return w->isDialog() || w->isUtility() || w->isModal();
}

// A dialog and the window it belongs to are one piece of work: the window
// behind a Save dialog is still the one being used, and so are the other
// dialogs open from the same window.
inline bool belongTogether(EffectWindow *a, EffectWindow *b)
{
    if (!a || !b) {
        return false;
    }
    if (a == b) {
        return true;
    }
    const QList<EffectWindow *> chainA = withMainWindows(a);
    const QList<EffectWindow *> chainB = withMainWindows(b);
    for (EffectWindow *w : chainA) {
        if (chainB.contains(w)) {
            return true;
        }
    }
    // Some dialogs name no window as theirs: Ark asks "File already exists"
    // over its extraction progress window with no link between the two. A
    // dialog from the same program is taken as part of the same work.
    return a->pid() > 0 && a->pid() == b->pid() && (isDialogLike(a) || isDialogLike(b));
}

}
