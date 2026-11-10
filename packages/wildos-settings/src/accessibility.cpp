#include "accessibility.h"

#include "accesscore.h"

#include <KSharedConfig>

#include <QDBusConnection>
#include <QFile>
#include <QStandardPaths>

Accessibility::Accessibility(QObject *parent)
    : QObject(parent)
{
    m_pointerColours = AccessCore::pointerColours();
    m_screenReaderInstalled = AccessCore::screenReaderInstalled();
    m_highContrastInstalled =
        !QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("color-schemes/%1.colors").arg(AccessCore::highContrastScheme())).isEmpty();
    m_monoAudioInstalled = QFile::exists(QStringLiteral("/usr/lib/systemd/user/wildos-mono-audio.service"));
    m_keyboardInstalled = !AccessCore::keyboardDesktopFile().isEmpty();
    m_windowEffectsInstalled = AccessCore::windowEffectsInstalled();
    m_windowEffectsSupported = m_windowEffectsInstalled && AccessCore::windowEffectsSupported();

    // Shortcuts, kcm_access, the Plasma keyboard indicator or another
    // Settings window can change any of these while the page is open.
    watch(QStringLiteral("kaccessrc"));
    watch(QStringLiteral("kdeglobals"));
    watch(QStringLiteral("kcminputrc"));
    watch(QStringLiteral("kwinrc"));
    watch(QStringLiteral("wildosrc"));
    QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.KWin"), QStringLiteral("/VirtualKeyboard"), QStringLiteral("org.kde.kwin.VirtualKeyboard"),
                                          QStringLiteral("enabledChanged"), this, SLOT(refresh()));

    m_monoAudio = AccessCore::monoAudio();
    refresh();
    refreshMagnifier();
}

void Accessibility::watch(const QString &file)
{
    KConfigWatcher::Ptr watcher = KConfigWatcher::create(KSharedConfig::openConfig(file, KConfig::NoGlobals));
    if (watcher) {
        connect(watcher.data(), &KConfigWatcher::configChanged, this, &Accessibility::refresh);
        m_watchers.append(watcher);
    }
}

QList<int> Accessibility::pointerSizes() const
{
    return AccessCore::pointerSizes();
}

void Accessibility::refresh()
{
    m_pointerColour = AccessCore::pointerColour();
    m_pointerSize = AccessCore::pointerSize();
    m_screenReader = AccessCore::screenReader();
    if (!m_highContrastBusy) {
        m_highContrast = AccessCore::highContrast();
    }
    m_visibleBell = AccessCore::visibleBell();
    m_onScreenKeyboard = AccessCore::onScreenKeyboard();
    m_stickyKeys = AccessCore::stickyKeys();
    m_slowKeys = AccessCore::slowKeys();
    m_slowKeysDelay = AccessCore::slowKeysDelay();
    m_bounceKeys = AccessCore::bounceKeys();
    m_bounceKeysDelay = AccessCore::bounceKeysDelay();
    m_outlineWindows = AccessCore::outlineWindows();
    m_blurBackground = AccessCore::blurBackground();
    m_blurStrength = AccessCore::blurStrength();
    m_blurDesktop = AccessCore::blurDesktop();
    m_strongerText = AccessCore::strongerText();
    Q_EMIT changed();
}

void Accessibility::refreshMagnifier()
{
    const bool on = AccessCore::magnifier();
    if (on != m_magnifier) {
        m_magnifier = on;
        Q_EMIT magnifierChanged();
    }
}

void Accessibility::setPointerColour(const QString &id)
{
    AccessCore::setPointer(id, m_pointerSize);
    m_pointerColour = id;
    Q_EMIT changed();
}

void Accessibility::setPointerSize(int size)
{
    AccessCore::setPointer(m_pointerColour, size);
    m_pointerSize = size;
    Q_EMIT changed();
}

void Accessibility::setMagnifier(bool on)
{
    AccessCore::setMagnifier(on);
    m_magnifier = on;
    Q_EMIT magnifierChanged();
}

void Accessibility::setScreenReader(bool on)
{
    AccessCore::setScreenReader(on);
    m_screenReader = on;
    Q_EMIT changed();
}

void Accessibility::setHighContrast(bool on)
{
    if (m_highContrastBusy) {
        return;
    }
    m_highContrastBusy = true;
    m_highContrast = on;
    Q_EMIT changed();
    AccessCore::setHighContrast(on, this, [this] {
        m_highContrastBusy = false;
        refresh();
    });
}

void Accessibility::setVisibleBell(bool on)
{
    AccessCore::setVisibleBell(on);
    m_visibleBell = on;
    Q_EMIT changed();
}

void Accessibility::setMonoAudio(bool on)
{
    AccessCore::setMonoAudio(on);
    m_monoAudio = on;
    Q_EMIT changed();
}

void Accessibility::setOnScreenKeyboard(bool on)
{
    m_onScreenKeyboard = on;
    Q_EMIT changed();
    AccessCore::setOnScreenKeyboard(on, this, [this] {
        refresh();
    });
}

void Accessibility::setStickyKeys(bool on)
{
    AccessCore::setStickyKeys(on);
    m_stickyKeys = on;
    Q_EMIT changed();
}

void Accessibility::setSlowKeys(bool on)
{
    AccessCore::setSlowKeys(on);
    m_slowKeys = on;
    Q_EMIT changed();
}

void Accessibility::setSlowKeysDelay(int ms)
{
    AccessCore::setSlowKeysDelay(ms);
    m_slowKeysDelay = ms;
    Q_EMIT changed();
}

void Accessibility::setBounceKeys(bool on)
{
    AccessCore::setBounceKeys(on);
    m_bounceKeys = on;
    Q_EMIT changed();
}

void Accessibility::setBounceKeysDelay(int ms)
{
    AccessCore::setBounceKeysDelay(ms);
    m_bounceKeysDelay = ms;
    Q_EMIT changed();
}

void Accessibility::setOutlineWindows(bool on)
{
    AccessCore::setOutlineWindows(on);
    m_outlineWindows = on;
    Q_EMIT changed();
}

void Accessibility::setBlurBackground(bool on)
{
    AccessCore::setBlurBackground(on);
    m_blurBackground = on;
    Q_EMIT changed();
}

void Accessibility::setBlurStrength(int step)
{
    AccessCore::setBlurStrength(step);
    m_blurStrength = AccessCore::blurStrength();
    Q_EMIT changed();
}

void Accessibility::setBlurDesktop(bool on)
{
    AccessCore::setBlurDesktop(on);
    m_blurDesktop = on;
    Q_EMIT changed();
}

void Accessibility::setStrongerText(bool on)
{
    AccessCore::setStrongerText(on);
    m_strongerText = on;
    Q_EMIT changed();
}
