#pragma once

#include <KConfigWatcher>

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>

// Times in the regional format with the clock chosen in Settings > Time and
// language > Date and time (12-hour, 24-hour, or the regional format's own)
// applied, for every Wild OS clock drawn in QML. The formats are properties
// so a binding that uses them follows a change in Settings straight away:
//
//     Qt.locale().toString(now, UI.TimeFormat.shortFormat)
//
// clockformat.h holds the rules, shared with the C++ apps.
class TimeFormat : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // 12, 24, or 0 for the regional format's own clock.
    Q_PROPERTY(int hours READ hours NOTIFY changed)
    // "h:mm AP", "HH:mm" and so on.
    Q_PROPERTY(QString shortFormat READ shortFormat NOTIFY changed)
    // The same with seconds after the minutes.
    Q_PROPERTY(QString secondsFormat READ secondsFormat NOTIFY changed)

public:
    explicit TimeFormat(QObject *parent = nullptr);

    int hours() const { return m_hours; }
    QString shortFormat() const;
    QString secondsFormat() const;

    // The time without AM or PM, for clocks drawn too large to carry it.
    // A binding that calls it follows a change when its time next ticks.
    Q_INVOKABLE QString withoutMarker(const QDateTime &when) const;

Q_SIGNALS:
    void changed();

private:
    void read();

    KConfigWatcher::Ptr m_watcher;
    int m_hours = 0;
};
