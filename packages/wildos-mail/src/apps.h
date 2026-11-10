#pragma once

#include <QObject>
#include <QProcess>
#include <QtQml/qqmlregistration.h>

#include "models.h"

// What Mail and Calendar share without starting the other's engine: the
// app rail's buttons (Mail and Calendar are separate windows with their own
// taskbar buttons, so moving between them starts, or brings forward, the
// other) and a person's initials and colour.
class Apps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    Q_INVOKABLE void open(const QString &app) const
    {
        QProcess::startDetached(app == QLatin1String("calendar") ? QStringLiteral("wildos-calendar") : QStringLiteral("wildos-mail"), {});
    }
    // Right-clicking the rail: Calendar in a window of its own. Mail has
    // one window, so for Mail this is the same as choosing it.
    Q_INVOKABLE void openWindow(const QString &app) const
    {
        if (app == QLatin1String("calendar")) {
            QProcess::startDetached(QStringLiteral("wildos-calendar"), {QStringLiteral("--new-window")});
        } else {
            open(app);
        }
    }
    Q_INVOKABLE static QString initialsOf(const QString &name)
    {
        return MessageModel::initials(name);
    }
    Q_INVOKABLE static int colourOf(const QString &address)
    {
        return MessageModel::colourOf(address);
    }
};
