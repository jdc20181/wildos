#pragma once

#include <QHash>
#include <QObject>

#include <libecal/libecal.h>

// Event and task reminders (MailCalMsNotify), from evolution-data-server's
// reminder watcher, which knows every calendar's and task list's reminders,
// snoozes and the ones missed while the PC was off. Runs at sign-in as
// wildos-calendar --reminders, without a window, in place of
// evolution-data-server's own GTK reminder program. Each shows as a
// notification that stays until it is snoozed or dismissed; choosing it
// opens Calendar at the event, or Tasks for a task.
class Reminders : public QObject
{
    Q_OBJECT

public:
    explicit Reminders(QObject *parent = nullptr);
    ~Reminders() override;

    // false when the calendar service can't be reached.
    bool start();

private Q_SLOTS:
    void onAction(uint id, const QString &action);
    void onClosed(uint id, uint reason);

private:
    static void onTriggered(EReminderWatcher *, const GSList *reminders, gboolean snoozed, gpointer self);
    void show(const EReminderData *data);

    ESourceRegistry *m_registry = nullptr;
    EReminderWatcher *m_watcher = nullptr;
    QHash<uint, EReminderData *> m_shown;
};
