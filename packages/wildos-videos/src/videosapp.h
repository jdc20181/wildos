#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QRect>

// The window of the one Videos process. Opening a video in Files while
// Videos runs hands it to this process, so a second one never starts.
//
// Picture in picture shrinks this same window and keeps it above the
// others (mpv draws into one render context, so a second window cannot
// show the picture too). On Wayland a window can neither stay on top nor
// place itself, so a short KWin script does both and reports where the
// window was, so it can go back there.
class VideosApp : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_CLASSINFO("D-Bus Interface", "org.wildos.videos.PictureInPicture")

public:
    // No default constructor: QML would make a second one instead of
    // calling create().
    explicit VideosApp(QObject *parent);
    static VideosApp *instance();
    static VideosApp *create(QQmlEngine *, QJSEngine *);

    void setMainWindow(QQuickWindow *window);
    // Acts on a command line: from this start, or handed over by a second
    // start while Videos is running.
    void handle(const QStringList &arguments, const QString &workingDirectory);
    // The files to open from a command line, made absolute.
    static QStringList filesFrom(const QStringList &arguments, const QString &workingDirectory);

    Q_INVOKABLE void showMain();
    // Shrinks the window to `size` in the bottom right corner, above the
    // others; or puts it back where and how big it was.
    Q_INVOKABLE void enterPictureInPicture(const QSize &size);
    Q_INVOKABLE void leavePictureInPicture(const QSize &fallback);

public Q_SLOTS:
    // From the KWin script: where the window was before it shrank,
    // "x,y,width,height".
    Q_SCRIPTABLE void remember(const QString &geometry);

private:
    void activate(QQuickWindow *window);
    void runKWinScript(const QString &source);

    QPointer<QQuickWindow> m_main;
    QRect m_before;
};
