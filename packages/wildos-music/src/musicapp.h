#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>

// The windows of the one Music process: the main window and the mini
// player. Opening songs from Files while Music runs hands them to this
// process, so a second one never starts.
class MusicApp : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool mini READ mini NOTIFY miniChanged)

public:
    // No default constructor: QML would make a second one instead of
    // calling create().
    explicit MusicApp(QObject *parent);
    static MusicApp *instance();
    static MusicApp *create(QQmlEngine *, QJSEngine *);

    void setEngine(QQmlEngine *engine);
    void setMainWindow(QQuickWindow *window);
    // Acts on a command line: from this start, or handed over by a second
    // start while Music is running.
    void handle(const QStringList &arguments, const QString &workingDirectory);

    bool mini() const;

    // Swaps the main window for the small always-on-top player.
    Q_INVOKABLE void showMini();
    // Brings back the main window, closing the mini player.
    Q_INVOKABLE void showMain();

Q_SIGNALS:
    void miniChanged();

private:
    void activate(QQuickWindow *window);
    void keepAbove(QQuickWindow *window);

    QQmlEngine *m_engine = nullptr;
    QPointer<QQuickWindow> m_main;
    QPointer<QQuickWindow> m_mini;
};
