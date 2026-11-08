#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QVariantList>

// Keyboard layouts, the key that switches between them, Caps Lock and key
// repeat, kept where KDE's keyboard module keeps them: the layouts and XKB
// options in kxkbrc (KWin rereads it on org.kde.keyboard reloadConfig) and
// repeat and Num Lock in kcminputrc, which KWin watches.
class Keyboard : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList layouts READ layouts NOTIFY changed)
    Q_PROPERTY(int current READ current NOTIFY currentChanged)
    Q_PROPERTY(QString switchKeys READ switchKeys NOTIFY changed)
    Q_PROPERTY(QString capsLock READ capsLock NOTIFY changed)
    Q_PROPERTY(int numLock READ numLock NOTIFY changed)
    Q_PROPERTY(bool repeat READ repeat NOTIFY changed)
    Q_PROPERTY(int repeatDelay READ repeatDelay NOTIFY changed)
    Q_PROPERTY(int repeatRate READ repeatRate NOTIFY changed)

public:
    explicit Keyboard(QObject *parent = nullptr);

    QVariantList layouts() const;
    int current() const { return m_current; }
    QString switchKeys() const;
    QString capsLock() const;
    int numLock() const { return m_numLock; }
    bool repeat() const { return m_repeat; }
    int repeatDelay() const { return m_repeatDelay; }
    int repeatRate() const { return m_repeatRate; }

    // Every layout and variant XKB knows, as { layout, variant, name },
    // sorted by name. Read once, on first use.
    Q_INVOKABLE QVariantList available();
    Q_INVOKABLE void add(const QString &layout, const QString &variant);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void use(int index);
    Q_INVOKABLE void setSwitchKeys(const QString &option);
    Q_INVOKABLE void setCapsLock(const QString &option);
    Q_INVOKABLE void setNumLock(int state);
    Q_INVOKABLE void setRepeat(bool on);
    Q_INVOKABLE void setRepeatDelay(int ms);
    Q_INVOKABLE void setRepeatRate(int perSecond);
    Q_INVOKABLE void reload();

Q_SIGNALS:
    void changed();
    void currentChanged();

private Q_SLOTS:
    void onLayoutChanged(uint index);
    void readCurrent();

private:
    struct Layout {
        QString layout;
        QString variant;
    };

    void loadNames();
    QString nameOf(const Layout &l) const;
    void saveLayouts();
    void saveRepeat();
    QString optionWithPrefix(const QStringList &prefixes) const;
    void replaceOptions(const QStringList &prefixes, const QString &option);

    QList<Layout> m_layouts;
    QStringList m_options;
    int m_current = 0;
    int m_numLock = 2;
    bool m_repeat = true;
    int m_repeatDelay = 600;
    int m_repeatRate = 25;

    bool m_namesLoaded = false;
    QHash<QString, QString> m_names;
    QVariantList m_available;
};
