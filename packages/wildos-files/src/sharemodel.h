#pragma once

#include <QList>
#include <QObject>
#include <QUrl>
#include <QVariantList>

namespace Purpose
{
class AlternativesModel;
}

// The Share button: the system's share targets (email, nearby phones,
// image hosts) for the selected files, limited to the ones that work
// without a setup form, since Files draws the choice itself.
class Sharer : public QObject
{
    Q_OBJECT

public:
    explicit Sharer(QObject *parent = nullptr);

    QVariantList targets(const QList<QUrl> &urls);
    void share(int row);

Q_SIGNALS:
    void finished(const QString &message);
    void failed(const QString &message);

private:
    Purpose::AlternativesModel *m_model;
};
