#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QQmlEngine>
#include <QRect>
#include <QTimer>
#include <memory>

class PipeWireRecord;
class Screencasting;
class ScreencastStream;
class QScreen;

// Screen recording, as Snipping Tool's record button does it: KWin streams
// the chosen area, window or screen into PipeWire (its screencast protocol,
// offered to programs whose desktop file lists zkde_screencast_unstable_v1)
// and KPipeWire encodes the stream to a video file.
class Recorder : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(bool finishing READ finishing NOTIFY recordingChanged)
    Q_PROPERTY(int seconds READ seconds NOTIFY secondsChanged)

public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder() override;

    bool available() const;
    bool recording() const { return m_recording; }
    bool finishing() const { return m_finishing; }
    int seconds() const { return m_elapsed.isValid() ? int(m_elapsed.elapsed() / 1000) : 0; }

    // Global logical coordinates.
    void recordArea(const QRect &area, qreal scale);
    void recordWindow(const QString &id);
    void recordScreen(QScreen *screen);
    Q_INVOKABLE void stop();

Q_SIGNALS:
    void recordingChanged();
    void secondsChanged();
    void finished(const QString &path);
    void failed(const QString &message);

private:
    void attach(ScreencastStream *stream);
    void dropStream();
    QString nextPath(const QString &extension) const;

    std::unique_ptr<Screencasting> m_casting;
    ScreencastStream *m_stream = nullptr;
    PipeWireRecord *m_record = nullptr;
    QString m_path;
    bool m_recording = false;
    bool m_finishing = false;
    QElapsedTimer m_elapsed;
    QTimer m_tick;
};
