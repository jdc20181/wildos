#pragma once

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

#include <libecal/libecal.h>

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(tasksLog)

class QQmlEngine;
class QJSEngine;

// Tasks (MailTasks.dc), on evolution-data-server's calendar service: every
// task list in its source registry (the one on this PC, Google's task lists
// and CalDAV VTODO lists, from the accounts added in Calendar), each task a
// VTODO. The service does the syncing; this keeps the tasks in memory for
// the lists and hands the service what was changed.
//
// Most of a task is standard iCalendar: SUMMARY, DESCRIPTION, DUE,
// STATUS and COMPLETED, PRIORITY (1 for Important, as Apple Reminders and
// Thunderbird read it), RRULE, a VALARM at an absolute time, ATTACH with
// the file inline, CREATED. Two things have no field:
//  - My day: X-WILDOS-MYDAY, the date it was added, so it empties itself
//    each morning as Microsoft To Do's does.
//  - Steps: one X-WILDOS-STEP per step, its text after "[ ] " or "[x] ".
// CalDAV servers keep X- properties with the task; Google's task lists keep
// only the title, notes, due date and done, so a task there loses the rest.
class TasksEngine : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    // True while a refresh asked of the accounts is still running, held for
    // at least a moment so a quick one still shows the icon turning.
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY readyChanged)
    // [{uid, name, account, color, readOnly, count}]: one per task list.
    Q_PROPERTY(QVariantList lists READ lists NOTIFY tasksChanged)
    Q_PROPERTY(QString defaultList READ defaultList NOTIFY tasksChanged)
    // Goes up whenever any task changes; lists bind to it.
    Q_PROPERTY(int revision READ revision NOTIFY tasksChanged)

public:
    static TasksEngine *instance();
    static TasksEngine *create(QQmlEngine *, QJSEngine *);
    ~TasksEngine() override;

    bool ready() const
    {
        return m_registry != nullptr;
    }
    QString problem() const
    {
        return m_problem;
    }
    QVariantList lists() const;
    QString defaultList() const;
    int revision() const
    {
        return m_revision;
    }

    // view is myday, important, planned, all, or a list's uid; text
    // narrows it to titles and notes containing it. done picks the open
    // tasks or the completed ones. Rows: [{key, title, listUid, listName,
    // done, important, myDay, due, dueText, dueState (overdue, today or
    // empty), reminder, steps, stepsDone, files}].
    Q_INVOKABLE QVariantList tasks(const QString &view, const QString &text, bool done, const QString &sort) const;
    // Open tasks in a view, for the counts beside the lists.
    Q_INVOKABLE int count(const QString &view) const;
    // One task in full: the row's fields and notes, steps [{text, done}],
    // reminderAt, repeat (none, daily, weekdays, weekly, monthly, yearly,
    // other), files [{name, size}], created.
    Q_INVOKABLE QVariantMap task(const QString &key) const;

    // A new task in the view's list (or the default one), with what the
    // view implies: My day, Important, or due today for Planned. Answered
    // with added(key).
    Q_INVOKABLE void addTask(const QString &title, const QString &view, const QVariantMap &extra);
    Q_INVOKABLE void setDone(const QString &key, bool done);
    Q_INVOKABLE void setImportant(const QString &key, bool important);
    Q_INVOKABLE void setMyDay(const QString &key, bool on);
    Q_INVOKABLE void setTitle(const QString &key, const QString &title);
    Q_INVOKABLE void setNotes(const QString &key, const QString &notes);
    // An invalid date clears it. allDay is a due day without a time.
    Q_INVOKABLE void setDue(const QString &key, const QDateTime &due, bool allDay);
    Q_INVOKABLE void setReminder(const QString &key, const QDateTime &at);
    Q_INVOKABLE void setRepeat(const QString &key, const QString &repeat);
    // [{text, done}], in order.
    Q_INVOKABLE void setSteps(const QString &key, const QVariantList &steps);
    Q_INVOKABLE void moveTo(const QString &key, const QString &listUid);
    Q_INVOKABLE void remove(const QString &key);
    Q_INVOKABLE void addFile(const QString &key, const QUrl &file);
    Q_INVOKABLE void removeFile(const QString &key, int index);
    // Writes the file out where it can be opened, and opens it.
    Q_INVOKABLE void openFile(const QString &key, int index);

    // A list on this PC; lists made on an account's own site or phone
    // arrive by themselves.
    Q_INVOKABLE void addList(const QString &name);
    Q_INVOKABLE void renameList(const QString &uid, const QString &name);
    Q_INVOKABLE void removeList(const QString &uid);
    Q_INVOKABLE void refresh();
    bool syncing() const { return m_syncing > 0 || m_syncHold.isActive(); }
    // Counts a refresh in and out; the callback ends it.
    void startRefresh(EClient *client);
    static void onRefreshed(GObject *, GAsyncResult *result, gpointer);
    int m_syncing = 0;
    QTimer m_syncHold;

Q_SIGNALS:
    void readyChanged();
    void tasksChanged();
    void syncingChanged();
    void notice(const QString &text);
    void added(const QString &key);

private:
    explicit TasksEngine(QObject *parent = nullptr);

    struct Task {
        QString key;
        QString listUid;
        QString uid;
        QString title;
        QString notes;
        bool done = false;
        bool important = false;
        QDate myDay;
        QDateTime due;
        bool dueAllDay = true;
        QDateTime reminder;
        QDateTime created;
        QDateTime completed;
        QString repeat = QStringLiteral("none");
        QList<QPair<bool, QString>> steps;
        QList<QPair<QString, qint64>> files;
    };
    struct List {
        QString uid;
        QString name;
        QString account;
        QString color;
        bool readOnly = false;
    };

    static void onRegistryReady(GObject *, GAsyncResult *result, gpointer self);
    static void onSourceChanged(ESourceRegistry *, ESource *, gpointer self);
    static void onClientConnected(GObject *, GAsyncResult *result, gpointer data);
    static void onViewReady(GObject *, GAsyncResult *result, gpointer data);
    static void onViewObjects(ECalClientView *, gpointer, gpointer self);

    void rebuildLists();
    void connectClient(ESource *source);
    void dropClient(const QString &uid);
    void startView(const QString &uid);
    void scheduleReload(const QString &uid);
    void reloadPending();
    void reload(const QString &uid);
    void flatten();
    void run(std::function<void()> work, std::function<void()> done);
    // Changes the task in memory with local straight away, then fetches it,
    // changes it with edit on a worker thread and hands it back.
    void change(const QString &key, std::function<void(Task &)> local, std::function<void(ECalComponent *)> edit);
    bool inView(const Task &t, const QString &view) const;
    QVariantMap row(const Task &t) const;
    ECalClient *client(const QString &uid) const;
    Task *find(const QString &key);
    const Task *find(const QString &key) const;
    ICalTimezone *zone() const;

    ESourceRegistry *m_registry = nullptr;
    GCancellable *m_cancel = nullptr;
    QString m_problem;
    QHash<QString, ECalClient *> m_clients;
    QHash<QString, ECalClientView *> m_views;
    QList<List> m_lists;
    QHash<QString, QList<Task>> m_perList;
    QList<Task> m_tasks;
    QHash<QString, int> m_byKey;
    QSet<QString> m_pending;
    QTimer m_reloadTimer;
    int m_revision = 0;
};
