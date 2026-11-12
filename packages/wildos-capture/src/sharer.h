#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantList>

namespace Purpose
{
class AlternativesModel;
}

// Share on a screenshot or recording toast: the system's share targets
// (email, Bluetooth, image hosts) for one file, as wildos-files' Share
// button does, limited to targets that need no setup form, since the
// Share window draws the choice itself.
class Sharer : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit Sharer(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList targets(const QUrl &file);
    Q_INVOKABLE void share(int row);

Q_SIGNALS:
    void finished(const QString &message);
    void failed(const QString &message);

private:
    Purpose::AlternativesModel *m_model;
};
