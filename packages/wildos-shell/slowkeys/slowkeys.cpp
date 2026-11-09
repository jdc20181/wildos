/*
    SPDX-FileCopyrightText: 2025 Martin Riethmayer <ripper@freakmail.de>
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "slowkeys.h"

#include <effect/effect.h>
#include <effect/effecthandler.h>

namespace WildOS
{

SlowKeys::SlowKeys()
    // Ahead of sticky keys, as in KWin's own order, so a brushed Shift is
    // never latched.
    : KWin::InputEventFilter(KWin::InputFilterOrder::BounceKeys)
    , m_configWatcher(KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("kaccessrc"))))
{
    const QLatin1String groupName("Keyboard");
    connect(m_configWatcher.get(), &KConfigWatcher::configChanged, this, [this, groupName](const KConfigGroup &group) {
        if (group.name() == groupName) {
            loadConfig(group);
        }
    });
    loadConfig(m_configWatcher->config()->group(groupName));
}

void SlowKeys::loadConfig(const KConfigGroup &group)
{
    KWin::input()->uninstallInputEventFilter(this);

    if (group.readEntry<bool>("SlowKeys", false)) {
        KWin::input()->installInputEventFilter(this);
        m_delay = std::chrono::milliseconds(group.readEntry<int>("SlowKeysDelay", 500));
        m_pressBeep = group.readEntry<bool>("SlowKeysPressBeep", false);
        m_acceptBeep = group.readEntry<bool>("SlowKeysAcceptBeep", false);
        m_rejectBeep = group.readEntry<bool>("SlowKeysRejectBeep", false);
    } else {
        m_keys.clear();
    }
}

void SlowKeys::ring() const
{
    if (KWin::effects) {
        if (auto effect = KWin::effects->provides(KWin::Effect::SystemBell)) {
            effect->perform(KWin::Effect::SystemBell, {});
        }
    }
}

bool SlowKeys::keyboardKey(KWin::KeyboardKeyEvent *event)
{
    const auto now = std::chrono::steady_clock::now();

    switch (event->state) {
    case KWin::KeyboardKeyState::Pressed:
    case KWin::KeyboardKeyState::Repeated: {
        const auto it = m_keys.find(event->key);
        if (it == m_keys.end()) {
            m_keys.insert(event->key, Key{now, 0});
            if (m_pressBeep) {
                ring();
            }
            return true;
        }
        if (now - it->pressed < m_delay) {
            if (m_rejectBeep) {
                ring();
            }
            return true;
        }
        if (m_acceptBeep && it->sent == 0) {
            ring();
        }
        // Everything before was held back, so the app sees a press first
        // and only then repeats.
        event->state = it->sent == 0 ? KWin::KeyboardKeyState::Pressed : KWin::KeyboardKeyState::Repeated;
        it->pressed = now;
        ++it->sent;
        return false;
    }
    case KWin::KeyboardKeyState::Released: {
        const auto it = m_keys.find(event->key);
        if (it == m_keys.end()) {
            return false;
        }
        const bool seen = it->sent != 0;
        m_keys.erase(it);
        return !seen;
    }
    }
    return false;
}

}

#include "moc_slowkeys.cpp"
