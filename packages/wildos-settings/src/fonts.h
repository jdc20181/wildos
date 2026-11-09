#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

// The fonts on this computer, as fontconfig sees them, after Windows 11's
// Personalisation > Fonts. Fonts you install go to ~/.local/share/fonts,
// the folder KDE's font installer uses for one account, and fc-cache is run
// on it so new apps find them. Fonts that came with the system are listed
// but cannot be removed here.
class Fonts : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // [{ family, styles, removable }], sorted by name.
    Q_PROPERTY(QVariantList families READ families NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit Fonts(QObject *parent = nullptr);

    QVariantList families() const { return m_families; }
    bool busy() const { return m_busy; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void install(const QList<QUrl> &files);
    Q_INVOKABLE void remove(const QString &family);

Q_SIGNALS:
    void changed();
    void busyChanged();
    // What happened, in a sentence for the page.
    void finished(const QString &title, const QString &message, bool failed);

private:
    void refreshCache(const QString &title, const QString &message, bool failed);
    void setBusy(bool busy);

    QVariantList m_families;
    QHash<QString, QStringList> m_userFiles;
    bool m_busy = false;
};
