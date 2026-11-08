#pragma once

#include <QObject>
#include <QQmlEngine>
#include <memory>

// Loads KDE settings modules so Wild OS pages can show them. Modules written
// in QML are created in the app's own engine and placed inside the Settings
// window; the few still built from widgets open in a window of their own.
class KcmHost : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static void setEngine(const std::shared_ptr<QQmlEngine> &engine);
    static KcmHost *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    Q_INVOKABLE bool available(const QString &pluginId) const;
    Q_INVOKABLE bool embeddable(const QString &pluginId) const;
    Q_INVOKABLE QObject *load(const QString &pluginId);
    Q_INVOKABLE QString lastError() const;
    Q_INVOKABLE void release(QObject *module);
    Q_INVOKABLE void apply(QObject *module);
    Q_INVOKABLE void revert(QObject *module);
    Q_INVOKABLE void openWindow(const QString &pluginId, const QString &title);

private:
    explicit KcmHost(QObject *parent = nullptr);

    std::shared_ptr<QQmlEngine> m_engine;
    QString m_lastError;
};
