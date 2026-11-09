#pragma once

#include <kscreen/types.h>

#include <QObject>
#include <QPoint>
#include <QQmlEngine>
#include <QSize>
#include <QTimer>
#include <QVariantList>

// Settings > System > Display. The screens are read and changed through
// libkscreen, the library KDE's own display module uses, so KWin keeps every
// change for the next sign-in exactly as if it had been made there. Each
// change is applied at once, as on Windows; a new resolution, refresh rate
// or HDR switch is kept only once the person says so, and switches back
// after 15 seconds otherwise.
//
// The countdown lives here rather than in the page: Settings rebuilds a page
// each time it is opened, and leaving Display mid-countdown must still
// switch back.
class Displays : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // False until KScreen has answered the first time.
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    // KScreen answered and its screens can be changed.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    // Connected screens in Windows' numbering, built-in first. Each is a map;
    // see describe() in display.cpp for the keys.
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
    // single (one screen connected), extend, duplicate or only (one of
    // several screens is on).
    Q_PROPERTY(QString arrangement READ arrangement NOTIFY changed)
    Q_PROPERTY(bool confirming READ confirming NOTIFY confirmChanged)
    Q_PROPERTY(int secondsLeft READ secondsLeft NOTIFY confirmChanged)

public:
    explicit Displays(QObject *parent = nullptr);

    bool ready() const { return m_ready; }
    bool available() const;
    QVariantList outputs() const { return m_outputs; }
    QString arrangement() const { return m_arrangement; }
    bool confirming() const { return bool(m_revert); }
    int secondsLeft() const { return m_secondsLeft; }

    // Asks KScreen for the screens again (Detect).
    Q_INVOKABLE void reload();

    // Where a screen dragged to x, y (desktop pixels) lands: against the
    // nearest edge of another screen, never overlapping one, with edges and
    // centres closer than `threshold` lined up.
    Q_INVOKABLE QPoint snap(int id, int x, int y, int threshold) const;
    Q_INVOKABLE void move(int id, int x, int y);

    Q_INVOKABLE void setPrimary(int id);
    // size is "3840x2160", as in each output's resolutions.
    Q_INVOKABLE void setResolution(int id, const QString &size);
    // rate is a value from the output's rates.
    Q_INVOKABLE void setRefreshRate(int id, const QString &rate);
    Q_INVOKABLE void setScale(int id, double scale);
    // KScreen's rotation: 1 none, 2 left, 4 inverted, 8 right.
    Q_INVOKABLE void setRotation(int id, int rotation);
    // extend, duplicate, only or off, for the screen `id`.
    Q_INVOKABLE void setArrangement(int id, const QString &mode);
    Q_INVOKABLE void setVrr(int id, bool on);
    Q_INVOKABLE void setHdr(int id, bool on);
    Q_INVOKABLE void setSdrBrightness(int id, int nits);

    Q_INVOKABLE void keep();
    Q_INVOKABLE void revert();

Q_SIGNALS:
    void changed();
    void confirmChanged();
    void failed(const QString &reason);

private:
    KScreen::OutputPtr output(int id) const;
    QList<KScreen::OutputPtr> ordered() const;
    QList<KScreen::OutputPtr> enabledOutputs() const;
    QVariantMap describe(const KScreen::OutputPtr &output, int number) const;
    void refresh();
    void apply();
    void preview(const KScreen::ConfigPtr &before);
    void resize(const KScreen::OutputPtr &output, const QSize &oldSize);
    void normalise();
    void lineUp();
    void switchOn(const KScreen::OutputPtr &output);
    void mirror(const KScreen::OutputPtr &replica, const KScreen::OutputPtr &source);
    QPoint snapped(const KScreen::OutputPtr &moving, const QPoint &dest, int threshold) const;
    void stopCountdown();

    KScreen::ConfigPtr m_config;
    // What to go back to when a previewed change is not kept.
    KScreen::ConfigPtr m_revert;
    QVariantList m_outputs;
    QString m_arrangement;
    bool m_ready = false;
    bool m_loading = false;
    bool m_reloadAgain = false;
    int m_applying = 0;
    int m_secondsLeft = 0;
    QTimer m_countdown;
    QTimer m_reloadSoon;
    QTimer m_applySoon;
};
