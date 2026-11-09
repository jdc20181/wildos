#pragma once

#include <KConfigWatcher>
#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>

// What the system decides for every Wild OS surface: the accent chosen in
// Settings > Personalisation and whether the session is dark or light. Both
// live in kdeglobals ([General] AccentColor and ColorScheme), which Settings
// writes with KConfig::Notify, so apps, plasmoids and the shell repaint the
// moment either changes.
class Theme : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QColor accent READ accent NOTIFY accentChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY darkChanged)
    Q_PROPERTY(QString colourScheme READ colourScheme NOTIFY darkChanged)
    // Settings > Accessibility > High contrast, which applies the
    // WildOSHighContrast scheme; Style then draws solid black surfaces,
    // white text and strong edges instead of glass.
    Q_PROPERTY(bool highContrast READ highContrast NOTIFY darkChanged)
    // Qt.application.font is a constant snapshot in QML, so a text size
    // change from Settings would only show after a restart; this follows
    // the application's font change events instead.
    Q_PROPERTY(qreal fontPointSize READ fontPointSize NOTIFY fontPointSizeChanged)
    // Settings > Accessibility > Reduce motion, which sets KDE's animation
    // speed to instant (kdeglobals [KDE] AnimationDurationFactor=0).
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY reducedMotionChanged)

public:
    explicit Theme(QObject *parent = nullptr);

    QColor accent() const { return m_accent; }
    bool dark() const { return m_dark; }
    QString colourScheme() const { return m_scheme; }
    bool highContrast() const { return m_scheme == QLatin1String("WildOSHighContrast"); }
    qreal fontPointSize() const;
    bool reducedMotion() const { return m_reducedMotion; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

Q_SIGNALS:
    void accentChanged();
    void darkChanged();
    void fontPointSizeChanged();
    void reducedMotionChanged();

private:
    void read();

    KConfigWatcher::Ptr m_watcher;
    QTimer m_settle;
    QColor m_accent;
    bool m_dark = true;
    bool m_reducedMotion = false;
    QString m_scheme;
};
