#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

// Step 6. Each switch is backed by the real thing it describes; where Wild
// OS has nothing a person can change yet, the switch is locked and shows
// what is actually true.
class Privacy : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool updateChecks READ updateChecks NOTIFY changed)
    Q_PROPERTY(bool updateChecksAvailable READ updateChecksAvailable CONSTANT)
    Q_PROPERTY(bool crashReports READ crashReports NOTIFY changed)
    Q_PROPERTY(bool crashReportsAvailable READ crashReportsAvailable CONSTANT)
    Q_PROPERTY(bool location READ location CONSTANT)
    Q_PROPERTY(bool webSearch READ webSearch CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY busyChanged)

public:
    explicit Privacy(QObject *parent = nullptr);

    bool updateChecks() const { return m_updateChecks; }
    bool updateChecksAvailable() const;
    bool crashReports() const;
    bool crashReportsAvailable() const;
    bool location() const;
    bool webSearch() const { return false; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }

    Q_INVOKABLE void setUpdateChecks(bool on);
    Q_INVOKABLE void setCrashReports(bool on);

    // In the setup session (Setup::planning) the system-wide update timers
    // change at once; what belongs to the new account is kept for apply.
    Q_INVOKABLE QVariantMap plan() const;
    void apply(const QVariantMap &plan);

Q_SIGNALS:
    void changed();
    void busyChanged();

private:
    bool readUpdateChecks() const;
    void setNotifierHidden(bool hidden);

    bool m_updateChecks = true;
    bool m_plannedCrashReports = false;
    bool m_busy = false;
    QString m_error;
};
