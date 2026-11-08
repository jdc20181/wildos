#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

#include <functional>

// Step 4 and the flow around it: the accent, the taskbar layout and the
// wallpaper the sheet sits on, plus the once-per-user flag. Changes go
// through the same tools and config keys wildos-settings uses, so the
// desktop and Settings agree with what was picked here.
class Setup : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QColor accent READ accent NOTIFY accentChanged)
    // WildOS (dark) or WildOSLight, as Settings > Personalisation sets it.
    Q_PROPERTY(QString colourScheme READ colourScheme NOTIFY colourSchemeChanged)
    Q_PROPERTY(bool automaticTheme READ automaticTheme NOTIFY colourSchemeChanged)
    Q_PROPERTY(QString wallpaper READ wallpaper CONSTANT)
    Q_PROPERTY(int taskbarLayout READ taskbarLayout NOTIFY taskbarLayoutChanged)
    Q_PROPERTY(bool taskbarLayoutKnown READ taskbarLayoutKnown NOTIFY taskbarLayoutChanged)
    Q_PROPERTY(QString firstName READ firstName CONSTANT)

public:
    explicit Setup(QObject *parent = nullptr);

    enum TaskbarLayout { Centred = 0, LeftAligned = 1 };
    Q_ENUM(TaskbarLayout)

    QColor accent() const { return m_accent; }
    QString colourScheme() const { return m_colourScheme; }
    bool automaticTheme() const { return m_automaticTheme; }
    QString wallpaper() const { return m_wallpaper; }
    int taskbarLayout() const { return m_taskbarLayout; }
    bool taskbarLayoutKnown() const { return m_taskbarLayoutKnown; }
    QString firstName() const;

    Q_INVOKABLE void setAccent(const QColor &colour);
    Q_INVOKABLE bool hasColourScheme(const QString &name) const;
    Q_INVOKABLE void setColourScheme(const QString &name);
    Q_INVOKABLE void setAutomaticTheme();
    Q_INVOKABLE void setTaskbarLayout(int layout);
    Q_INVOKABLE void finish();
    Q_INVOKABLE void quit();

    /* In the first boot setup session there is no taskbar yet and the
     * account the choices are for has not signed in. Choices are then only
     * recorded (plan) and the account's own welcome applies them (apply). */
    static void setPlanning(bool planning);
    static bool planning();
    Q_INVOKABLE QVariantMap plan() const;
    /* Applies a plan's accent, colour scheme and taskbar layout, waiting
     * for the taskbar to start. Calls done when finished. */
    void apply(const QVariantMap &plan, std::function<void()> done);

    // The flag file and the places a first-run flow must stay away from.
    static bool alreadyDone();
    static QString reasonNotToRun();

Q_SIGNALS:
    void accentChanged();
    void colourSchemeChanged();
    void taskbarLayoutChanged();

private:
    void readAccent();
    void readColourScheme();
    static void writeThemeMode(const QString &mode);
    void readWallpaper();
    void readTaskbarLayout();
    static QString evaluatePlasmaScript(const QString &script, bool *ok = nullptr);
    static bool hasProgram(const QString &program);

    QColor m_accent;
    QString m_colourScheme;
    bool m_automaticTheme = false;
    QString m_wallpaper;
    int m_taskbarLayout = Centred;
    bool m_taskbarLayoutKnown = false;
};
