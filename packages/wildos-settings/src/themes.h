#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

#include <KConfigWatcher>

class QProcess;

// Settings > Personalisation > Themes (SettingsThemes.dc). A theme is one
// of /usr/share/wildos/themes/<id>.json (wildos-branding): the wallpaper,
// light or dark, the accent and the glass tint in one go. Applying one
// writes the choice to wildosrc [Theme] Id, which WildOS.UI's Theme reads
// for the glass and wildos-auto-theme for the Plasma style, and sets the
// scheme, accent and wallpaper through System like the pages for each.
//
// It also holds the taskbar underglow and RGB lighting settings, which the
// underglow renderer (KWin) and the OpenRGB mirroring read from wildosrc:
//   [Underglow] Enabled, Colour=accent|custom|rgb, Custom=#rrggbb,
//               Effect=steady|breathing|flowing
//   [Lighting]  Rgb
// [Underglow] FromTheme marks underglow that is on only because a theme
// turned it on, so the next theme can turn it off again.
class Themes : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Every installed theme in its order: {id, name, description, mode,
    // accent, accentName, textOnAccent, glass, taskbarGlass, wallpaper,
    // moreWallpaperNames, underglow, isDefault}.
    Q_PROPERTY(QVariantList themes READ themes CONSTANT)
    Q_PROPERTY(QString current READ current NOTIFY currentChanged)
    Q_PROPERTY(QVariantMap currentTheme READ currentTheme NOTIFY currentChanged)

    Q_PROPERTY(bool underglow READ underglow NOTIFY underglowChanged)
    Q_PROPERTY(QString underglowColour READ underglowColour NOTIFY underglowChanged)
    Q_PROPERTY(QColor underglowCustom READ underglowCustom NOTIFY underglowChanged)
    Q_PROPERTY(QString underglowEffect READ underglowEffect NOTIFY underglowChanged)
    Q_PROPERTY(bool lightingRgb READ lightingRgb NOTIFY underglowChanged)
    // Keyboards, mice and other lighting OpenRGB can drive; 0 when there
    // are none or the counting tool is missing.
    Q_PROPERTY(int rgbDevices READ rgbDevices NOTIFY rgbDevicesChanged)

public:
    explicit Themes(QObject *parent = nullptr);

    QVariantList themes() const { return m_themes; }
    QString current() const { return m_current; }
    QVariantMap currentTheme() const;

    bool underglow() const { return m_underglow; }
    QString underglowColour() const { return m_underglowColour; }
    QColor underglowCustom() const { return m_underglowCustom; }
    QString underglowEffect() const { return m_underglowEffect; }
    bool lightingRgb() const { return m_lightingRgb; }
    int rgbDevices() const { return m_rgbDevices; }

    Q_INVOKABLE void apply(const QString &id);

    Q_INVOKABLE void setUnderglow(bool on);
    // accent, custom or rgb; the effect follows (rgb flows, the others are
    // steady unless breathing was chosen).
    Q_INVOKABLE void setUnderglowColour(const QString &colour);
    Q_INVOKABLE void setUnderglowCustom(const QColor &colour);
    Q_INVOKABLE void setUnderglowEffect(const QString &effect);
    Q_INVOKABLE void setLightingRgb(bool on);
    Q_INVOKABLE void refreshRgbDevices();

    // A theme accent's name ("Neon"), or "" for any other colour.
    Q_INVOKABLE QString accentName(const QColor &colour) const;

Q_SIGNALS:
    void currentChanged();
    void underglowChanged();
    void rgbDevicesChanged();

private:
    void loadThemes();
    void read();
    void writeUnderglow(const QString &key, const QVariant &value);

    QVariantList m_themes;
    QString m_current;
    bool m_underglow = false;
    bool m_underglowFromTheme = false;
    QString m_underglowColour;
    QColor m_underglowCustom;
    QString m_underglowEffect;
    bool m_lightingRgb = false;
    int m_rgbDevices = 0;
    QProcess *m_rgbProbe = nullptr;
    KConfigWatcher::Ptr m_watcher;
};
