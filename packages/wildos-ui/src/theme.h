#pragma once

#include <KConfigWatcher>
#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>

// What the system decides for every Wild OS surface: the accent chosen in
// Settings > Personalisation, whether the session is dark or light, and the
// theme's glass tint. The first two live in kdeglobals ([General]
// AccentColor and ColorScheme), the theme in wildosrc ([Theme] Id, one of
// /usr/share/wildos/themes/<id>.json). Settings writes all of them with
// KConfig::Notify, so apps, plasmoids and the shell repaint the moment any
// changes.
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
    // KDE's animation speed (AnimationDurationFactor) as a multiplier for
    // every Style duration, so Wild OS apps keep pace with the window
    // manager. 0 with reduced motion.
    Q_PROPERTY(qreal animationFactor READ animationFactor NOTIFY reducedMotionChanged)
    // Settings > Accessibility > Stronger text on glass (wildosrc
    // [Accessibility] StrongerText): Style makes glass nearly solid and
    // muted text brighter.
    Q_PROPERTY(bool strongerText READ strongerText NOTIFY strongerTextChanged)

    // The theme picked in Settings > Personalisation > Themes ("dawn" when
    // none has been).
    Q_PROPERTY(QString themeId READ themeId NOTIFY themeChanged)
    // Every glass surface is tinted relative to glassReference, the colour
    // of the reference surface (window glass in dark mode, the theme cards'
    // light glass in light mode). glassTint is the theme's glass while the
    // session is in the theme's own mode, otherwise the reference, so a
    // light theme switched to dark by hand falls back to the stock glass.
    Q_PROPERTY(QColor glassReference READ glassReference NOTIFY themeChanged)
    Q_PROPERTY(QColor glassTint READ glassTint NOTIFY themeChanged)
    // A taskbar glass of its own (Overclock's is darker than its windows).
    Q_PROPERTY(bool hasTaskbarTint READ hasTaskbarTint NOTIFY themeChanged)
    Q_PROPERTY(QColor taskbarTint READ taskbarTint NOTIFY themeChanged)
    // The theme's text colours hold only for the theme's own accent in its
    // own mode; any other accent gets the computed ones.
    Q_PROPERTY(bool themeAccent READ themeAccent NOTIFY themeChanged)
    Q_PROPERTY(QColor textOnAccent READ textOnAccent NOTIFY themeChanged)
    Q_PROPERTY(QColor accentText READ accentText NOTIFY themeChanged)

public:
    explicit Theme(QObject *parent = nullptr);

    QColor accent() const { return m_accent; }
    bool dark() const { return m_dark; }
    QString colourScheme() const { return m_scheme; }
    bool highContrast() const { return m_scheme == QLatin1String("WildOSHighContrast"); }
    qreal fontPointSize() const;
    bool reducedMotion() const { return m_reducedMotion; }
    qreal animationFactor() const { return m_animationFactor; }
    bool strongerText() const { return m_strongerText; }

    QString themeId() const { return m_themeId; }
    QColor glassReference() const;
    QColor glassTint() const;
    bool hasTaskbarTint() const { return themeMode() && m_taskbarTint.isValid(); }
    QColor taskbarTint() const { return m_taskbarTint; }
    bool themeAccent() const;
    QColor textOnAccent() const { return m_textOnAccent; }
    QColor accentText() const { return m_accentText; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

Q_SIGNALS:
    void accentChanged();
    void darkChanged();
    void fontPointSizeChanged();
    void reducedMotionChanged();
    void strongerTextChanged();
    void themeChanged();

private:
    void read();
    void readTheme();
    void readAccessibility();
    // The session is in the mode the theme was made for.
    bool themeMode() const;

    KConfigWatcher::Ptr m_watcher;
    KConfigWatcher::Ptr m_wildosWatcher;
    QTimer m_settle;
    QColor m_accent;
    bool m_dark = true;
    bool m_reducedMotion = false;
    qreal m_animationFactor = 1.0;
    bool m_strongerText = false;
    QString m_scheme;

    QString m_themeId;
    QString m_themeMode;
    QColor m_themeAccent;
    QColor m_glass;
    QColor m_taskbarTint;
    QColor m_textOnAccent;
    QColor m_accentText;
};
