#include "gestures.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QHash>

namespace
{
const QString groupName = QStringLiteral("Gestures");

// Kept the same as the effect's own defaults (wildos-shell
// effects/gestures/gestures.cpp), which apply when nothing is written.
const QHash<QString, QString> defaults = {
    {QStringLiteral("ThreeUp"), QStringLiteral("taskview")},
    {QStringLiteral("ThreeDown"), QStringLiteral("desktop")},
    {QStringLiteral("ThreeSideways"), QStringLiteral("desktops")},
    {QStringLiteral("FourUp"), QStringLiteral("taskview")},
    {QStringLiteral("FourDown"), QStringLiteral("desktop")},
    {QStringLiteral("FourSideways"), QStringLiteral("desktops")},
    {QStringLiteral("PageSwipe"), QStringLiteral("history")},
};

// Keys from the first Gestures page, which had one set of swipes for the
// number of fingers Fingers chose. The effect carries them over the same way.
const QStringList oldKeys = {
    QStringLiteral("Fingers"), QStringLiteral("SwipeUp"), QStringLiteral("SwipeDown"),
    QStringLiteral("SwipeSideways"), QStringLiteral("PinchIn"), QStringLiteral("PinchOut"),
};
}

Gestures::Gestures(QObject *parent)
    : QObject(parent)
    , m_watcher(KConfigWatcher::create(KSharedConfig::openConfig(QStringLiteral("wildosrc"))))
{
    connect(m_watcher.get(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &changedGroup) {
        if (changedGroup.name() == groupName) {
            Q_EMIT changed();
        }
    });
}

KConfigGroup Gestures::group() const
{
    return m_watcher->config()->group(groupName);
}

void Gestures::write(const char *key, const QVariant &value)
{
    KConfigGroup g = group();
    g.writeEntry(key, value, KConfig::Notify);
    g.sync();
    Q_EMIT changed();
}

bool Gestures::threeFingerDrag() const
{
    return group().readEntry("ThreeFingerDrag", false);
}

void Gestures::setThreeFingerDrag(bool on)
{
    if (on != threeFingerDrag()) {
        write("ThreeFingerDrag", on);
    }
}

QString Gestures::action(const QString &gesture) const
{
    const KConfigGroup g = group();
    QString fallback = defaults.value(gesture, QStringLiteral("nothing"));
    const bool three = gesture.startsWith(QLatin1String("Three"));
    if (three || gesture.startsWith(QLatin1String("Four"))) {
        const bool oldFour = g.readEntry("Fingers", 3) == 4;
        const QString direction = gesture.mid(three ? 5 : 4);
        if (three == !oldFour) {
            fallback = g.readEntry(QStringLiteral("Swipe%1").arg(direction).toUtf8().constData(), fallback);
        } else if (three) {
            fallback = QStringLiteral("nothing");
        }
    }
    return g.readEntry(gesture.toUtf8().constData(), fallback);
}

void Gestures::setAction(const QString &gesture, const QString &action)
{
    if (defaults.contains(gesture) && action != this->action(gesture)) {
        write(gesture.toUtf8().constData(), action);
    }
}

bool Gestures::isDefault() const
{
    if (threeFingerDrag()) {
        return false;
    }
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
        if (action(it.key()) != it.value()) {
            return false;
        }
    }
    return true;
}

// Each default written out rather than the group deleted, because a
// deletion does not reach KWin's and the apps' config watchers.
void Gestures::reset()
{
    KConfigGroup g = group();
    g.writeEntry("ThreeFingerDrag", false, KConfig::Notify);
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
        g.writeEntry(it.key().toUtf8().constData(), it.value(), KConfig::Notify);
    }
    for (const QString &key : oldKeys) {
        g.deleteEntry(key.toUtf8().constData(), KConfig::Notify);
    }
    g.sync();
    Q_EMIT changed();
}
