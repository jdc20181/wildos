/*
    SPDX-FileCopyrightText: 2025 Martin Riethmayer <ripper@freakmail.de>
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <input.h>
#include <input_event.h>
#include <plugin.h>

#include <KConfigWatcher>

#include <QHash>

#include <chrono>

namespace WildOS
{

// KWin's slow keys filter from after Plasma 6.5, on KWin 6.3's API. It
// reads the keys kcm_access and Settings write (kaccessrc [Keyboard]
// SlowKeys, SlowKeysDelay and the three beeps), so it follows Settings >
// Accessibility live, in the session, on the lock screen and on the
// sign-in screen alike. A press is held back until the key has been down
// for the delay; KWin's key repeat delivers the moment it is accepted.
class SlowKeys : public KWin::Plugin, public KWin::InputEventFilter
{
    Q_OBJECT

public:
    SlowKeys();

    bool keyboardKey(KWin::KeyboardKeyEvent *event) override;

private:
    struct Key {
        std::chrono::steady_clock::time_point pressed;
        uint32_t sent = 0;
    };

    void loadConfig(const KConfigGroup &group);
    void ring() const;

    KConfigWatcher::Ptr m_configWatcher;
    std::chrono::milliseconds m_delay{500};
    bool m_pressBeep = false;
    bool m_acceptBeep = false;
    bool m_rejectBeep = false;
    QHash<int, Key> m_keys;
};

}
