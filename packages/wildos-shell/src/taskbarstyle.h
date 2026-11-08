#pragma once

#include <KConfigWatcher>

#include <QObject>
#include <QQmlEngine>

// The taskbar style chosen in Settings > Personalisation: docked along the
// bottom of the screen (the default, as Windows does it) or floating glass
// panels. Settings writes wildosrc [Taskbar] Style; the panel follows the
// change straight away.
class TaskbarStyle : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool floating READ floating NOTIFY floatingChanged)

public:
    explicit TaskbarStyle(QObject *parent = nullptr);

    bool floating() const { return m_floating; }

Q_SIGNALS:
    void floatingChanged();

private:
    void read();

    KConfigWatcher::Ptr m_watcher;
    bool m_floating = false;
};
