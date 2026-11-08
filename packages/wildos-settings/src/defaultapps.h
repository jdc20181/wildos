#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Which app opens what: web links, email, pictures, music, video, PDFs,
// text and folders. The choice is written to mimeapps.list, the standard
// every Linux app reads, so it holds outside Wild OS's own apps too.
class DefaultApps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList categories READ categories NOTIFY changed)

public:
    explicit DefaultApps(QObject *parent = nullptr);

    // [{ id, title, description, icon, current, apps: [{ value, text }] }]
    QVariantList categories() const { return m_categories; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setDefault(const QString &category, const QString &storageId);

Q_SIGNALS:
    void changed();

private:
    QVariantList m_categories;
};
