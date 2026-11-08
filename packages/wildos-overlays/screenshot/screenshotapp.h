#pragma once

#include <QImage>
#include <QJsonArray>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QRectF>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include "recorder.h"
#include "sharer.h"

class FrozenImages;
class Grabber;
class KNotification;

// Wild OS Screenshot, Win+Shift+S: the screen freezes under a dimmed
// overlay with Snipping Tool's toolbar (rectangle, window, full screen,
// freehand, record, delay). What you pick goes to the clipboard and to
// Pictures/Screenshots, and a toast offers Mark up and Share.
//
// One process at a time: pressing the shortcut again while a toast is up
// reaches the running one through D-Bus. It stays while any of its toasts
// is still in the notification list, so their buttons keep working.
class ScreenshotApp : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool selecting READ selecting NOTIFY selectingChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool recordMode READ recordMode WRITE setRecordMode NOTIFY modeChanged)
    Q_PROPERTY(int delay READ delay WRITE setDelay NOTIFY delayChanged)
    Q_PROPERTY(int countdown READ countdown NOTIFY countdownChanged)
    Q_PROPERTY(int serial READ serial NOTIFY selectingChanged)
    Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
    Q_PROPERTY(bool canRecord READ canRecord CONSTANT)
    Q_PROPERTY(Recorder *recorder READ recorder CONSTANT)
    Q_PROPERTY(Sharer *sharer READ sharer CONSTANT)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    // The toolbar goes on the main screen only.
    Q_PROPERTY(QString primaryScreen READ primaryScreen NOTIFY selectingChanged)

public:
    explicit ScreenshotApp(QObject *parent = nullptr);
    ~ScreenshotApp() override;

    static ScreenshotApp *instance();

    FrozenImages *frozen() const { return m_frozen; }

    bool selecting() const { return m_selecting; }
    QString mode() const { return m_mode; }
    void setMode(const QString &mode);
    bool recordMode() const { return m_recordMode; }
    void setRecordMode(bool on);
    int delay() const { return m_delay; }
    void setDelay(int seconds);
    int countdown() const { return m_countdown; }
    int serial() const { return m_serial; }
    QVariantList windows() const { return m_windows; }
    bool canRecord() const;
    Recorder *recorder() const { return m_recorder; }
    Sharer *sharer() const { return m_sharer; }
    QString message() const { return m_message; }
    QString primaryScreen() const;

    // From the command line, or a second launch: --record, --mode <name>,
    // --markup <file>.
    void handle(const QStringList &arguments);

    // The overlay's choices, in each screen's own logical coordinates.
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void takeArea(const QString &screen, const QRectF &area);
    Q_INVOKABLE void takeFreehand(const QString &screen, const QVariantList &points);
    Q_INVOKABLE void takeWindow(const QString &id, const QRectF &globalArea);
    Q_INVOKABLE void takeScreen(const QString &screen);
    // With a delay set: the overlay goes, a countdown runs, and the screen
    // is frozen again when it ends.
    Q_INVOKABLE void startCountdown();

    // The markup window's results.
    Q_INVOKABLE void copyImageFile(const QUrl &file);
    Q_INVOKABLE void copyImage(const QVariant &image);
    Q_INVOKABLE void openMarkup(const QUrl &file);
    Q_INVOKABLE void openShare(const QUrl &file);
    Q_INVOKABLE void openFile(const QUrl &file);
    Q_INVOKABLE void showInFolder(const QUrl &file);
    Q_INVOKABLE void windowClosed();

Q_SIGNALS:
    void selectingChanged();
    void modeChanged();
    void delayChanged();
    void countdownChanged();
    void windowsChanged();
    void messageChanged();
    void markupRequested(const QUrl &file);
    void shareRequested(const QUrl &file);

private:
    void begin();
    void freeze();
    void finish(const QImage &image);
    void recordingFinished(const QString &path);
    void notifyFailure(const QString &title, const QString &text);
    void setMessage(const QString &message);
    QString nextPath() const;
    void quitWhenIdle();
    QImage crop(const QString &screen, const QRectF &area) const;

    FrozenImages *m_frozen;
    Grabber *m_grabber;
    Recorder *m_recorder;
    Sharer *m_sharer;
    bool m_selecting = false;
    QString m_mode = QStringLiteral("rectangle");
    bool m_recordMode = false;
    int m_delay = 0;
    int m_countdown = 0;
    int m_serial = 0;
    int m_openWindows = 0;
    QVariantList m_windows;
    QList<QPointer<KNotification>> m_notifications;
    QTimer m_countdownTimer;
    QTimer m_idleTimer;
    QString m_message;
};
