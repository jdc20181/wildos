#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>

// The window's ties to the rest of the system: recordings handed over by
// Files or a second start, and bringing the window forward.
class VoiceApp : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // A recording to show once the window is up, from the command line.
    Q_PROPERTY(QString pendingPath READ pendingPath NOTIFY pendingPathChanged)

public:
    explicit VoiceApp(QObject *parent);
    static VoiceApp *instance();
    static VoiceApp *create(QQmlEngine *, QJSEngine *);

    QString pendingPath() const { return m_pendingPath; }
    void setMainWindow(QQuickWindow *window) { m_window = window; }
    void handle(const QStringList &arguments, const QString &workingDirectory);
    Q_INVOKABLE void showMain();
    Q_INVOKABLE void takePendingPath();

Q_SIGNALS:
    void pendingPathChanged();

private:
    QPointer<QQuickWindow> m_window;
    QString m_pendingPath;
};
