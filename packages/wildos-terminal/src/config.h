#pragma once

#include <KConfigWatcher>
#include <KSharedConfig>
#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Terminal's settings, kept in wildosterminalrc. Every window, in every
// Terminal process, follows a change at once: writes are announced and
// each process reads them back.
class Config : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString scheme READ scheme WRITE setScheme NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY changed)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY changed)
    Q_PROPERTY(QString cursorShape READ cursorShape WRITE setCursorShape NOTIFY changed)
    Q_PROPERTY(bool cursorBlink READ cursorBlink WRITE setCursorBlink NOTIFY changed)
    Q_PROPERTY(int opacity READ opacity WRITE setOpacity NOTIFY changed)
    Q_PROPERTY(bool blur READ blur WRITE setBlur NOTIFY changed)
    Q_PROPERTY(bool boldBright READ boldBright WRITE setBoldBright NOTIFY changed)
    Q_PROPERTY(QString defaultProfile READ defaultProfile WRITE setDefaultProfile NOTIFY changed)
    Q_PROPERTY(int launchColumns READ launchColumns WRITE setLaunchColumns NOTIFY changed)
    Q_PROPERTY(int launchRows READ launchRows WRITE setLaunchRows NOTIFY changed)
    Q_PROPERTY(bool launchMaximised READ launchMaximised WRITE setLaunchMaximised NOTIFY changed)
    Q_PROPERTY(bool copyOnSelect READ copyOnSelect WRITE setCopyOnSelect NOTIFY changed)
    Q_PROPERTY(bool pasteOnRightClick READ pasteOnRightClick WRITE setPasteOnRightClick NOTIFY changed)
    Q_PROPERTY(bool ctrlCopyPaste READ ctrlCopyPaste WRITE setCtrlCopyPaste NOTIFY changed)
    Q_PROPERTY(bool warnMultilinePaste READ warnMultilinePaste WRITE setWarnMultilinePaste NOTIFY changed)
    Q_PROPERTY(bool confirmCloseTabs READ confirmCloseTabs WRITE setConfirmCloseTabs NOTIFY changed)
    Q_PROPERTY(int scrollback READ scrollback WRITE setScrollback NOTIFY changed)

    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QVariantList schemes READ schemes CONSTANT)
    Q_PROPERTY(QStringList monospaceFonts READ monospaceFonts CONSTANT)
    Q_PROPERTY(QColor accent READ accent NOTIFY accentChanged)
    Q_PROPERTY(QString homePath READ homePath CONSTANT)
    Q_PROPERTY(QString defaultShell READ defaultShell CONSTANT)

public:
    static Config *instance();
    static Config *create(QQmlEngine *, QJSEngine *);

    QString scheme() const { return m_scheme; }
    QString fontFamily() const { return m_fontFamily; }
    int fontSize() const { return m_fontSize; }
    QString cursorShape() const { return m_cursorShape; }
    bool cursorBlink() const { return m_cursorBlink; }
    int opacity() const { return m_opacity; }
    bool blur() const { return m_blur; }
    bool boldBright() const { return m_boldBright; }
    QString defaultProfile() const;
    int launchColumns() const { return m_launchColumns; }
    int launchRows() const { return m_launchRows; }
    bool launchMaximised() const { return m_launchMaximised; }
    bool copyOnSelect() const { return m_copyOnSelect; }
    bool pasteOnRightClick() const { return m_pasteOnRightClick; }
    bool ctrlCopyPaste() const { return m_ctrlCopyPaste; }
    bool warnMultilinePaste() const { return m_warnMultilinePaste; }
    bool confirmCloseTabs() const { return m_confirmCloseTabs; }
    int scrollback() const { return m_scrollback; }

    void setScheme(const QString &v);
    void setFontFamily(const QString &v);
    void setFontSize(int v);
    void setCursorShape(const QString &v);
    void setCursorBlink(bool v);
    void setOpacity(int v);
    void setBlur(bool v);
    void setBoldBright(bool v);
    void setDefaultProfile(const QString &v);
    void setLaunchColumns(int v);
    void setLaunchRows(int v);
    void setLaunchMaximised(bool v);
    void setCopyOnSelect(bool v);
    void setPasteOnRightClick(bool v);
    void setCtrlCopyPaste(bool v);
    void setWarnMultilinePaste(bool v);
    void setConfirmCloseTabs(bool v);
    void setScrollback(int v);

    QVariantList profiles() const { return m_profiles; }
    QVariantList schemes() const;
    QStringList monospaceFonts() const;
    QColor accent() const { return m_accent; }
    QString homePath() const;
    QString defaultShell() const;

    // { id, name, command, directory, icon }; the first profile when the
    // id is unknown, so a tab can always open.
    Q_INVOKABLE QVariantMap profile(const QString &id) const;
    Q_INVOKABLE QString addProfile();
    Q_INVOKABLE QString duplicateProfile(const QString &id);
    Q_INVOKABLE void removeProfile(const QString &id);
    Q_INVOKABLE void setProfileValue(const QString &id, const QString &key, const QString &value);
    Q_INVOKABLE void moveProfile(const QString &id, int step);

    QFont terminalFont() const;
    QString schemePath() const;
    QColor schemeBackground() const;

Q_SIGNALS:
    void changed();
    void profilesChanged();
    void accentChanged();

private:
    explicit Config(QObject *parent = nullptr);
    void read();
    void readProfiles();
    void writeProfiles();
    void readAccent();
    void write(const char *key, const QVariant &value);
    QString newId() const;

    KSharedConfigPtr m_config;
    KConfigWatcher::Ptr m_watcher;
    KConfigWatcher::Ptr m_globals;

    QString m_scheme;
    QString m_fontFamily;
    int m_fontSize = 11;
    QString m_cursorShape;
    bool m_cursorBlink = true;
    int m_opacity = 100;
    bool m_blur = true;
    bool m_boldBright = true;
    QString m_defaultProfile;
    int m_launchColumns = 120;
    int m_launchRows = 30;
    bool m_launchMaximised = false;
    bool m_copyOnSelect = false;
    bool m_pasteOnRightClick = false;
    bool m_ctrlCopyPaste = true;
    bool m_warnMultilinePaste = true;
    bool m_confirmCloseTabs = true;
    int m_scrollback = 10000;
    QVariantList m_profiles;
    QColor m_accent;
};
