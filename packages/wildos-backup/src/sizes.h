#pragma once

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>

#include <atomic>

class QThread;

// How much each part of What's included holds, worked out on a thread of
// its own (walking a home folder can take a minute) and kept in
// ~/.cache/wildos-backup so the Overview has numbers at once next time.
class Sizes : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(double home READ home NOTIFY changed)
    Q_PROPERTY(double mail READ mail NOTIFY changed)
    Q_PROPERTY(double settings READ settings NOTIFY changed)
    Q_PROPERTY(bool working READ working NOTIFY changed)

public:
    explicit Sizes(QObject *parent = nullptr);
    ~Sizes() override;

    double home() const
    {
        return m_home;
    }
    double mail() const
    {
        return m_mail;
    }
    double settings() const
    {
        return m_settings;
    }
    bool working() const
    {
        return m_working;
    }

    // Again, unless the cached numbers are recent and nothing changed.
    Q_INVOKABLE void refresh(bool force = false);

Q_SIGNALS:
    void changed();

private:
    void loadCache();
    void done(double home, double mail, double settings, const QString &key);
    QString key() const;

    double m_home = -1;
    double m_mail = -1;
    double m_settings = -1;
    bool m_working = false;
    bool m_again = false;
    QDateTime m_at;
    QString m_cacheKey;
    QTimer m_settle;
    std::atomic_bool m_cancel{false};
    QThread *m_thread = nullptr;
};
