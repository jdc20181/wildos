#pragma once

#include <QObject>
#include <QProcess>
#include <QtQml/qqmlregistration.h>

#include "models.h"

// What Mail and Calendar share without starting the other's engine: the
// app rail's buttons (Calendar's own window and Mail's, which also holds
// People and Tasks, have their own taskbar buttons, so moving between them
// starts, or brings forward, the other) and a person's initials and colour.
class Apps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    Q_INVOKABLE void open(const QString &app) const
    {
        if (app == QLatin1String("calendar")) {
            QProcess::startDetached(QStringLiteral("wildos-calendar"), {});
        } else if (app == QLatin1String("people") || app == QLatin1String("tasks")) {
            // People and Tasks are tabs of Mail's window.
            QProcess::startDetached(QStringLiteral("wildos-mail"), {QStringLiteral("--") + app});
        } else {
            QProcess::startDetached(QStringLiteral("wildos-mail"), {});
        }
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
