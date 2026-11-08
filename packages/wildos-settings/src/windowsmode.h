#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

class QDBusInterface;

// Settings > Windows apps > Windows mode (WinModeSettings.dc). Everything
// comes from the Windows mode service, org.wildos.WindowsMode1 (package
// wildos-winmode): its status is one JSON object,
// re-read whenever the service says it changed. Calls that can fail answer
// with a sentence to show, or nothing.
class WindowsMode : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Whether wildos-winmode is installed at all.
    Q_PROPERTY(bool installed READ installed CONSTANT)
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)
    // none, setup, failed or ready; empty until the service answers
    Q_PROPERTY(QString state READ state NOTIFY statusChanged)
    // The memory limits this PC can offer: [{value, text}]
    Q_PROPERTY(QVariantList memoryChoices READ memoryChoices CONSTANT)

public:
    explicit WindowsMode(QObject *parent = nullptr);

    bool installed() const;
    QVariantMap status() const { return m_status; }
    QString state() const { return m_status.value(QStringLiteral("state")).toString(); }
    QVariantList memoryChoices() const;

    Q_INVOKABLE QString setOption(const QString &key, const QString &value);
    Q_INVOKABLE QString setProductKey(const QString &key);
    Q_INVOKABLE bool keyShapeValid(const QString &key) const;
    Q_INVOKABLE QString checkUpdates();
    Q_INVOKABLE QString openDesktop();
    Q_INVOKABLE QString repair();
    Q_INVOKABLE QString remove();
    // The setup app (9.1 to 9.7).
    Q_INVOKABLE void openSetup();
    // "27.4 GB" from bytes; "" for nothing.
    Q_INVOKABLE QString size(double bytes) const;

Q_SIGNALS:
    void statusChanged();

private Q_SLOTS:
    void refresh();

private:
    QString call(const QString &method, const QVariantList &args = {}, int timeoutMs = 25000);

    QDBusInterface *m_service = nullptr;
    QVariantMap m_status;
};
