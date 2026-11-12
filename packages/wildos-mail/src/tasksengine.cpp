#include "tasksengine.h"
#include "formats.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QMimeDatabase>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimeZone>

#include <algorithm>
#include <memory>

Q_LOGGING_CATEGORY(tasksLog, "wildos.tasks", QtInfoMsg)

namespace
{
const QChar sep(0x1f);
const char *const myDayName = "X-WILDOS-MYDAY";
const char *const stepName = "X-WILDOS-STEP";
// Big enough for a receipt or a photo; the whole task travels with it to
// every device, so not a film.
const qint64 largestFile = 5 * 1024 * 1024;

QString str(const gchar *s)
{
    return s ? QString::fromUtf8(s) : QString();
}

QString takeStr(gchar *s)
{
    const QString out = str(s);
    g_free(s);
    return out;
}

QString keyOf(const QString &list, const QString &uid)
{
    return list + sep + uid;
}

bool splitKey(const QString &key, QString *list, QString *uid)
{
    const int at = key.indexOf(sep);
    if (at < 0) {
        return false;
    }
    *list = key.left(at);
    *uid = key.mid(at + 1);
    return true;
}

ICalTimezone *systemZone()
{
    ICalTimezone *z = i_cal_timezone_get_builtin_timezone(QTimeZone::systemTimeZoneId().constData());
    return z ? z : i_cal_timezone_get_utc_timezone();
}

QDateTime toDateTime(ICalTime *time)
{
    if (!time || i_cal_time_is_null_time(time) || !i_cal_time_is_valid_time(time)) {
        return {};
    }
    if (i_cal_time_is_date(time)) {
        return QDateTime(QDate(i_cal_time_get_year(time), i_cal_time_get_month(time), i_cal_time_get_day(time)), QTime(0, 0));
    }
    ICalTimezone *z = i_cal_time_get_timezone(time);
    if (!z && !i_cal_time_is_utc(time)) {
        z = systemZone();
    }
    const time_t secs = i_cal_time_as_timet_with_zone(time, z ? z : i_cal_timezone_get_utc_timezone());
    return QDateTime::fromSecsSinceEpoch(qint64(secs));
}

ICalTime *utcNow()
{
    return i_cal_time_new_current_with_zone(i_cal_timezone_get_utc_timezone());
}

QString repeatOf(ICalComponent *icomp)
{
    ICalProperty *p = i_cal_component_get_first_property(icomp, I_CAL_RRULE_PROPERTY);
    if (!p) {
        return QStringLiteral("none");
    }
    QString repeat = QStringLiteral("other");
    if (ICalRecurrence *r = i_cal_property_get_rrule(p)) {
        const QString text = takeStr(i_cal_recurrence_to_string(r));
        switch (i_cal_recurrence_get_freq(r)) {
        case I_CAL_DAILY_RECURRENCE:
            repeat = QStringLiteral("daily");
            break;
        case I_CAL_WEEKLY_RECURRENCE:
            repeat = text.contains(QLatin1String("BYDAY=MO,TU,WE,TH,FR")) ? QStringLiteral("weekdays") : QStringLiteral("weekly");
            break;
        case I_CAL_MONTHLY_RECURRENCE:
            repeat = QStringLiteral("monthly");
            break;
        case I_CAL_YEARLY_RECURRENCE:
            repeat = QStringLiteral("yearly");
            break;
        default:
            break;
        }
        g_object_unref(r);
    }
    g_object_unref(p);
    return repeat;
}

QString ruleFor(const QString &repeat)
{
    if (repeat == QLatin1String("daily")) {
        return QStringLiteral("FREQ=DAILY");
    }
    if (repeat == QLatin1String("weekdays")) {
        return QStringLiteral("FREQ=WEEKLY;BYDAY=MO,TU,WE,TH,FR");
    }
    if (repeat == QLatin1String("weekly")) {
        return QStringLiteral("FREQ=WEEKLY");
    }
    if (repeat == QLatin1String("monthly")) {
        return QStringLiteral("FREQ=MONTHLY");
    }
    if (repeat == QLatin1String("yearly")) {
        return QStringLiteral("FREQ=YEARLY");
    }
    return {};
}

QList<ICalProperty *> propertiesOf(ICalComponent *icomp, ICalPropertyKind kind, const char *xName = nullptr)
{
    QList<ICalProperty *> out;
    for (ICalProperty *p = i_cal_component_get_first_property(icomp, kind); p; p = i_cal_component_get_next_property(icomp, kind)) {
        if (xName && qstricmp(i_cal_property_get_x_name(p), xName) != 0) {
            g_object_unref(p);
            continue;
        }
        out << p;
    }
    return out;
}

void removeProperties(ICalComponent *icomp, const QList<ICalProperty *> &props)
{
    for (ICalProperty *p : props) {
        i_cal_component_remove_property(icomp, p);
        g_object_unref(p);
    }
}

QString fileNameOf(ICalProperty *p, ICalAttach *attach)
{
    QString name = takeStr(i_cal_property_get_parameter_as_string(p, "X-FILENAME"));
    if (name.isEmpty()) {
        name = takeStr(i_cal_property_get_parameter_as_string(p, "FILENAME"));
    }
    if (name.isEmpty() && attach && i_cal_attach_get_is_url(attach)) {
        name = QUrl(str(i_cal_attach_get_url(attach))).fileName();
    }
    return name.isEmpty() ? QObject::tr("File") : name;
}
}

TasksEngine *TasksEngine::instance()
{
    static TasksEngine *engine = new TasksEngine();
    return engine;
}

TasksEngine *TasksEngine::create(QQmlEngine *, QJSEngine *)
{
    TasksEngine *engine = instance();
    QJSEngine::setObjectOwnership(engine, QJSEngine::CppOwnership);
    return engine;
}

TasksEngine::TasksEngine(QObject *parent)
    : QObject(parent)
{
    m_syncHold.setSingleShot(true);
    m_syncHold.setInterval(700);
    connect(&m_syncHold, &QTimer::timeout, this, &TasksEngine::syncingChanged);
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(150);
    connect(&m_reloadTimer, &QTimer::timeout, this, &TasksEngine::reloadPending);
    m_cancel = g_cancellable_new();
    e_source_registry_new(m_cancel, &TasksEngine::onRegistryReady, this);
}

TasksEngine::~TasksEngine()
{
    g_cancellable_cancel(m_cancel);
    for (ECalClientView *view : std::as_const(m_views)) {
        e_cal_client_view_stop(view, nullptr);
        g_object_unref(view);
    }
    for (ECalClient *client : std::as_const(m_clients)) {
        if (client) {
            g_object_unref(client);
        }
    }
    g_clear_object(&m_registry);
    g_clear_object(&m_cancel);
}

ICalTimezone *TasksEngine::zone() const
{
    return systemZone();
}

void TasksEngine::onRegistryReady(GObject *, GAsyncResult *result, gpointer data)
{
    auto *self = static_cast<TasksEngine *>(data);
    GError *error = nullptr;
    ESourceRegistry *registry = e_source_registry_new_finish(result, &error);
    if (!registry) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            qCWarning(tasksLog) << "the calendar service didn't start:" << (error ? error->message : "");
            self->m_problem = tr("The tasks service didn't start.");
            Q_EMIT self->readyChanged();
        }
        g_clear_error(&error);
        return;
    }
    self->m_registry = registry;
    for (const char *signal : {"source-added", "source-removed", "source-changed", "source-enabled", "source-disabled"}) {
        g_signal_connect(registry, signal, G_CALLBACK(&TasksEngine::onSourceChanged), self);
    }
    self->rebuildLists();
    Q_EMIT self->readyChanged();
}

void TasksEngine::onSourceChanged(ESourceRegistry *, ESource *, gpointer data)
{
    auto *self = static_cast<TasksEngine *>(data);
    QMetaObject::invokeMethod(self, &TasksEngine::rebuildLists, Qt::QueuedConnection);
}

void TasksEngine::rebuildLists()
{
    if (!m_registry) {
        return;
    }
    QList<List> lists;
    QSet<QString> present;
    GList *sources = e_source_registry_list_enabled(m_registry, E_SOURCE_EXTENSION_TASK_LIST);
    for (GList *l = sources; l; l = l->next) {
        auto *source = E_SOURCE(l->data);
        List list;
        list.uid = str(e_source_get_uid(source));
        list.name = str(e_source_get_display_name(source));
        if (list.uid == QLatin1String("system-task-list")) {
            list.name = tr("Tasks");
        }
        auto *ext = E_SOURCE_SELECTABLE(e_source_get_extension(source, E_SOURCE_EXTENSION_TASK_LIST));
        list.color = takeStr(e_source_selectable_dup_color(ext));
        const QString parentUid = str(e_source_get_parent(source));
        if (!parentUid.isEmpty() && !parentUid.endsWith(QLatin1String("-stub"))) {
            if (ESource *parent = e_source_registry_ref_source(m_registry, parentUid.toUtf8().constData())) {
                list.account = str(e_source_get_display_name(parent));
                g_object_unref(parent);
            }
        }
        ECalClient *c = m_clients.value(list.uid);
        list.readOnly = c && e_client_is_readonly(E_CLIENT(c));
        lists << list;
        present.insert(list.uid);
        if (!m_clients.contains(list.uid)) {
            connectClient(source);
        }
    }
    g_list_free_full(sources, g_object_unref);

    std::stable_sort(lists.begin(), lists.end(), [](const List &a, const List &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    bool dropped = false;
    for (const QString &uid : m_clients.keys()) {
        if (!present.contains(uid)) {
            dropClient(uid);
            dropped = true;
        }
    }
    m_lists = lists;
    if (dropped) {
        flatten();
    } else {
        ++m_revision;
        Q_EMIT tasksChanged();
    }
}

void TasksEngine::connectClient(ESource *source)
{
    const QString uid = str(e_source_get_uid(source));
    m_clients.insert(uid, nullptr);
    e_cal_client_connect(source, E_CAL_CLIENT_SOURCE_TYPE_TASKS, 30, m_cancel, &TasksEngine::onClientConnected, new QString(uid));
}

void TasksEngine::onClientConnected(GObject *, GAsyncResult *result, gpointer data)
{
    const QString uid = *static_cast<QString *>(data);
    delete static_cast<QString *>(data);
    GError *error = nullptr;
    EClient *client = e_cal_client_connect_finish(result, &error);
    TasksEngine *self = instance();
    if (!client) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            qCWarning(tasksLog) << "couldn't open task list" << uid << (error ? error->message : "");
            self->m_clients.remove(uid);
        }
        g_clear_error(&error);
        return;
    }
    if (!self->m_clients.contains(uid)) {
        g_object_unref(client);
        return;
    }
    auto *cal = E_CAL_CLIENT(client);
    e_cal_client_set_default_timezone(cal, self->zone());
    self->m_clients.insert(uid, cal);
    self->rebuildLists();
    self->startView(uid);
    self->reload(uid);
}

void TasksEngine::dropClient(const QString &uid)
{
    if (ECalClientView *view = m_views.take(uid)) {
        g_signal_handlers_disconnect_by_data(view, this);
        e_cal_client_view_stop(view, nullptr);
        g_object_unref(view);
    }
    if (ECalClient *client = m_clients.take(uid)) {
        g_object_unref(client);
    }
    m_perList.remove(uid);
}

// Only a sign that the list changed; its tasks are fetched again.
void TasksEngine::startView(const QString &uid)
{
    ECalClient *client = m_clients.value(uid);
    if (!client) {
        return;
    }
    e_cal_client_get_view(client, "#t", m_cancel, &TasksEngine::onViewReady, new QString(uid));
}

void TasksEngine::onViewReady(GObject *object, GAsyncResult *result, gpointer data)
{
    const QString uid = *static_cast<QString *>(data);
    delete static_cast<QString *>(data);
    ECalClientView *view = nullptr;
    GError *error = nullptr;
    if (!e_cal_client_get_view_finish(E_CAL_CLIENT(object), result, &view, &error)) {
        g_clear_error(&error);
        return;
    }
    TasksEngine *self = instance();
    if (!self->m_clients.value(uid)) {
        g_object_unref(view);
        return;
    }
    if (ECalClientView *old = self->m_views.take(uid)) {
        g_signal_handlers_disconnect_by_data(old, self);
        e_cal_client_view_stop(old, nullptr);
        g_object_unref(old);
    }
    e_cal_client_view_set_flags(view, E_CAL_CLIENT_VIEW_FLAGS_NONE, nullptr);
    for (const char *signal : {"objects-added", "objects-modified", "objects-removed"}) {
        g_signal_connect(view, signal, G_CALLBACK(&TasksEngine::onViewObjects), self);
    }
    e_cal_client_view_start(view, nullptr);
    self->m_views.insert(uid, view);
}

void TasksEngine::onViewObjects(ECalClientView *view, gpointer, gpointer data)
{
    auto *self = static_cast<TasksEngine *>(data);
    for (auto it = self->m_views.cbegin(); it != self->m_views.cend(); ++it) {
        if (it.value() == view) {
            self->scheduleReload(it.key());
            return;
        }
    }
}

void TasksEngine::scheduleReload(const QString &uid)
{
    m_pending.insert(uid);
    m_reloadTimer.start();
}

void TasksEngine::reloadPending()
{
    const QSet<QString> pending = m_pending;
    m_pending.clear();
    for (const QString &uid : pending) {
        reload(uid);
    }
}

void TasksEngine::run(std::function<void()> work, std::function<void()> done)
{
    QPointer<TasksEngine> self(this);
    QThreadPool::globalInstance()->start([self, work, done]() {
        work();
        if (done && self) {
            QMetaObject::invokeMethod(self, done, Qt::QueuedConnection);
        }
    });
}

void TasksEngine::reload(const QString &uid)
{
    ECalClient *client = m_clients.value(uid);
    if (!client) {
        return;
    }
    g_object_ref(client);
    auto result = std::make_shared<QList<Task>>();
    run([client, uid, result]() {
        GSList *objects = nullptr;
        GError *error = nullptr;
        if (!e_cal_client_get_object_list_sync(client, "#t", &objects, nullptr, &error)) {
            qCWarning(tasksLog) << "couldn't read task list" << uid << (error ? error->message : "");
            g_clear_error(&error);
        }
        for (GSList *l = objects; l; l = l->next) {
            auto *icomp = static_cast<ICalComponent *>(l->data);
            if (i_cal_component_isa(icomp) != I_CAL_VTODO_COMPONENT) {
                continue;
            }
            Task t;
            t.listUid = uid;
            t.uid = str(i_cal_component_get_uid(icomp));
            t.key = keyOf(uid, t.uid);
            t.title = str(i_cal_component_get_summary(icomp)).trimmed();
            t.notes = str(i_cal_component_get_description(icomp));
            t.done = i_cal_component_get_status(icomp) == I_CAL_STATUS_COMPLETED;
            if (ICalProperty *p = i_cal_component_get_first_property(icomp, I_CAL_COMPLETED_PROPERTY)) {
                ICalTime *when = i_cal_property_get_completed(p);
                t.completed = toDateTime(when);
                t.done = true;
                g_clear_object(&when);
                g_object_unref(p);
            }
            if (ICalProperty *p = i_cal_component_get_first_property(icomp, I_CAL_PRIORITY_PROPERTY)) {
                const int priority = i_cal_property_get_priority(p);
                t.important = priority >= 1 && priority <= 4;
                g_object_unref(p);
            }
            ICalTime *due = i_cal_component_get_due(icomp);
            t.due = toDateTime(due);
            t.dueAllDay = due && i_cal_time_is_date(due);
            g_clear_object(&due);
            if (ICalProperty *p = i_cal_component_get_first_property(icomp, I_CAL_CREATED_PROPERTY)) {
                ICalTime *when = i_cal_property_get_created(p);
                t.created = toDateTime(when);
                g_clear_object(&when);
                g_object_unref(p);
            }
            t.myDay = QDate::fromString(takeStr(e_cal_util_component_dup_x_property(icomp, myDayName)), Qt::ISODate);
            for (ICalProperty *p : propertiesOf(icomp, I_CAL_X_PROPERTY, stepName)) {
                const QString v = str(i_cal_property_get_x(p));
                const bool stepDone = v.startsWith(QLatin1String("[x]"), Qt::CaseInsensitive);
                const QString text = v.startsWith(QLatin1Char('[')) && v.size() >= 3 && v.at(2) == QLatin1Char(']') ? v.mid(3).trimmed() : v.trimmed();
                t.steps << qMakePair(stepDone, text);
                g_object_unref(p);
            }
            t.repeat = repeatOf(icomp);
            if (ICalComponent *alarm = i_cal_component_get_first_component(icomp, I_CAL_VALARM_COMPONENT)) {
                if (ICalProperty *p = i_cal_component_get_first_property(alarm, I_CAL_TRIGGER_PROPERTY)) {
                    if (ICalTrigger *trigger = i_cal_property_get_trigger(p)) {
                        ICalTime *at = i_cal_trigger_get_time(trigger);
                        if (at && !i_cal_time_is_null_time(at)) {
                            t.reminder = toDateTime(at);
                        } else if (t.due.isValid()) {
                            ICalDuration *d = i_cal_trigger_get_duration(trigger);
                            t.reminder = t.due.addSecs(d ? i_cal_duration_as_int(d) : 0);
                            g_clear_object(&d);
                        }
                        g_clear_object(&at);
                        g_object_unref(trigger);
                    }
                    g_object_unref(p);
                }
                g_object_unref(alarm);
            }
            for (ICalProperty *p : propertiesOf(icomp, I_CAL_ATTACH_PROPERTY)) {
                ICalAttach *attach = i_cal_property_get_attach(p);
                qint64 size = 0;
                if (attach && !i_cal_attach_get_is_url(attach)) {
                    size = qint64(qstrlen(i_cal_attach_get_data(attach))) * 3 / 4;
                }
                t.files << qMakePair(fileNameOf(p, attach), size);
                g_clear_object(&attach);
                g_object_unref(p);
            }
            result->append(t);
        }
        g_slist_free_full(objects, g_object_unref);
        g_object_unref(client);
    }, [this, uid, result]() {
        if (!m_clients.value(uid)) {
            return;
        }
        m_perList.insert(uid, *result);
        flatten();
    });
}

void TasksEngine::flatten()
{
    m_tasks.clear();
    m_byKey.clear();
    for (const QList<Task> &list : std::as_const(m_perList)) {
        m_tasks << list;
    }
    for (int i = 0; i < m_tasks.size(); ++i) {
        m_byKey.insert(m_tasks[i].key, i);
    }
    ++m_revision;
    Q_EMIT tasksChanged();
}

ECalClient *TasksEngine::client(const QString &uid) const
{
    return m_clients.value(uid);
}

TasksEngine::Task *TasksEngine::find(const QString &key)
{
    const int i = m_byKey.value(key, -1);
    return i >= 0 ? &m_tasks[i] : nullptr;
}

const TasksEngine::Task *TasksEngine::find(const QString &key) const
{
    const int i = m_byKey.value(key, -1);
    return i >= 0 ? &m_tasks[i] : nullptr;
}

QVariantList TasksEngine::lists() const
{
    QHash<QString, int> counts;
    for (const Task &t : m_tasks) {
        if (!t.done) {
            ++counts[t.listUid];
        }
    }
    QVariantList out;
    for (const List &l : m_lists) {
        out << QVariantMap{
            {QStringLiteral("uid"), l.uid},
            {QStringLiteral("name"), l.name},
            {QStringLiteral("account"), l.account},
            {QStringLiteral("color"), l.color},
            {QStringLiteral("readOnly"), l.readOnly},
            {QStringLiteral("count"), counts.value(l.uid)},
        };
    }
    return out;
}

QString TasksEngine::defaultList() const
{
    if (m_registry) {
        if (ESource *source = e_source_registry_ref_default_task_list(m_registry)) {
            const QString uid = str(e_source_get_uid(source));
            g_object_unref(source);
            ECalClient *c = m_clients.value(uid);
            if (c && !e_client_is_readonly(E_CLIENT(c)) && uid != QLatin1String("system-task-list")) {
                return uid;
            }
        }
    }
    // An account's list before this PC's, so a new task reaches the phone.
    for (const List &l : m_lists) {
        ECalClient *c = m_clients.value(l.uid);
        if (!l.account.isEmpty() && c && !e_client_is_readonly(E_CLIENT(c))) {
            return l.uid;
        }
    }
    return QStringLiteral("system-task-list");
}

bool TasksEngine::inView(const Task &t, const QString &view) const
{
    if (view == QLatin1String("myday")) {
        return t.myDay == QDate::currentDate();
    }
    if (view == QLatin1String("important")) {
        return t.important;
    }
    if (view == QLatin1String("planned")) {
        return t.due.isValid();
    }
    if (view == QLatin1String("all")) {
        return true;
    }
    return t.listUid == view;
}

QVariantMap TasksEngine::row(const Task &t) const
{
    QString listName;
    for (const List &l : m_lists) {
        if (l.uid == t.listUid) {
            listName = l.name;
        }
    }
    QString dueText;
    QString dueState;
    if (t.due.isValid()) {
        const QDate today = QDate::currentDate();
        const QDate d = t.due.date();
        if (d == today) {
            dueText = tr("Today");
        } else if (d == today.addDays(1)) {
            dueText = tr("Tomorrow");
        } else if (d == today.addDays(-1)) {
            dueText = tr("Yesterday");
        } else if (d > today && d < today.addDays(7)) {
            dueText = Formats::names().dayName(d.dayOfWeek(), QLocale::ShortFormat);
        } else {
            dueText = Formats::shortDayAndMonth(d);
        }
        if (!t.dueAllDay) {
            dueText += QLatin1Char(' ') + Formats::time(t.due);
        }
        if (!t.done) {
            dueState = d < today ? QStringLiteral("overdue") : d == today ? QStringLiteral("today") : QString();
        }
    }
    int stepsDone = 0;
    for (const auto &s : t.steps) {
        stepsDone += s.first ? 1 : 0;
    }
    return {
        {QStringLiteral("key"), t.key},
        {QStringLiteral("title"), t.title.isEmpty() ? tr("(No title)") : t.title},
        {QStringLiteral("listUid"), t.listUid},
        {QStringLiteral("listName"), listName},
        {QStringLiteral("done"), t.done},
        {QStringLiteral("important"), t.important},
        {QStringLiteral("myDay"), t.myDay == QDate::currentDate()},
        {QStringLiteral("due"), t.due},
        {QStringLiteral("dueAllDay"), t.dueAllDay},
        {QStringLiteral("dueText"), dueText},
        {QStringLiteral("dueState"), dueState},
        {QStringLiteral("reminder"), t.reminder.isValid() && !t.done},
        {QStringLiteral("repeat"), t.repeat},
        {QStringLiteral("steps"), int(t.steps.size())},
        {QStringLiteral("stepsDone"), stepsDone},
        {QStringLiteral("files"), int(t.files.size())},
        {QStringLiteral("readOnly"), m_clients.value(t.listUid) ? bool(e_client_is_readonly(E_CLIENT(m_clients.value(t.listUid)))) : true},
    };
}

QVariantList TasksEngine::tasks(const QString &view, const QString &text, bool done, const QString &sort) const
{
    const QString needle = text.trimmed();
    QList<const Task *> picked;
    for (const Task &t : m_tasks) {
        if (t.done != done || !inView(t, view)) {
            continue;
        }
        if (!needle.isEmpty() && !t.title.contains(needle, Qt::CaseInsensitive) && !t.notes.contains(needle, Qt::CaseInsensitive)) {
            continue;
        }
        picked << &t;
    }
    std::sort(picked.begin(), picked.end(), [&sort, done](const Task *a, const Task *b) {
        if (done) {
            return a->completed > b->completed;
        }
        if (sort == QLatin1String("due")) {
            if (a->due.isValid() != b->due.isValid()) {
                return a->due.isValid();
            }
            if (a->due != b->due) {
                return a->due < b->due;
            }
        } else if (sort == QLatin1String("title")) {
            return a->title.compare(b->title, Qt::CaseInsensitive) < 0;
        } else if (sort == QLatin1String("important")) {
            if (a->important != b->important) {
                return a->important;
            }
        }
        // Newest first, as a task added goes to the top.
        return a->created > b->created;
    });
    QVariantList out;
    for (const Task *t : std::as_const(picked)) {
        out << row(*t);
    }
    return out;
}

int TasksEngine::count(const QString &view) const
{
    return int(std::count_if(m_tasks.cbegin(), m_tasks.cend(), [this, &view](const Task &t) {
        return !t.done && inView(t, view);
    }));
}

QVariantMap TasksEngine::task(const QString &key) const
{
    const Task *t = find(key);
    if (!t) {
        return {};
    }
    QVariantMap m = row(*t);
    m.insert(QStringLiteral("rawTitle"), t->title);
    m.insert(QStringLiteral("notes"), t->notes);
    QVariantList steps;
    for (const auto &s : t->steps) {
        steps << QVariantMap{{QStringLiteral("text"), s.second}, {QStringLiteral("done"), s.first}};
    }
    m.insert(QStringLiteral("steps"), steps);
    m.insert(QStringLiteral("reminderAt"), t->reminder);
    QVariantList files;
    for (const auto &f : t->files) {
        files << QVariantMap{{QStringLiteral("name"), f.first}, {QStringLiteral("size"), f.second}};
    }
    m.insert(QStringLiteral("files"), files);
    m.insert(QStringLiteral("created"), t->created);
    m.insert(QStringLiteral("completed"), t->completed);
    return m;
}

namespace
{
void setText(ECalComponent *comp, const QString &title)
{
    ECalComponentText *summary = e_cal_component_text_new(title.toUtf8().constData(), nullptr);
    e_cal_component_set_summary(comp, summary);
    e_cal_component_text_free(summary);
}

void setNotesOf(ECalComponent *comp, const QString &notes)
{
    GSList *descriptions = notes.trimmed().isEmpty() ? nullptr : g_slist_prepend(nullptr, e_cal_component_text_new(notes.toUtf8().constData(), nullptr));
    e_cal_component_set_descriptions(comp, descriptions);
    g_slist_free_full(descriptions, reinterpret_cast<GDestroyNotify>(e_cal_component_text_free));
}

ICalTime *icalTime(const QDateTime &t, bool allDay, ICalTimezone *zone)
{
    if (allDay) {
        ICalTime *d = i_cal_time_new_null_time();
        i_cal_time_set_date(d, t.date().year(), t.date().month(), t.date().day());
        i_cal_time_set_is_date(d, TRUE);
        return d;
    }
    return i_cal_time_new_from_timet_with_zone(time_t(t.toSecsSinceEpoch()), FALSE, zone);
}

void setDueOf(ECalComponent *comp, const QDateTime &due, bool allDay, ICalTimezone *zone)
{
    if (!due.isValid()) {
        e_cal_component_set_due(comp, nullptr);
        return;
    }
    const gchar *tzid = allDay ? nullptr : i_cal_timezone_get_tzid(zone);
    ECalComponentDateTime *dt = e_cal_component_datetime_new_take(icalTime(due, allDay, zone), g_strdup(tzid));
    e_cal_component_set_due(comp, dt);
    // A repeating task repeats from its start, which RFC 5545 asks for.
    if (e_cal_component_has_rrules(comp)) {
        e_cal_component_set_dtstart(comp, dt);
    }
    e_cal_component_datetime_free(dt);
}

void setDoneOf(ECalComponent *comp, bool done)
{
    if (done) {
        e_cal_component_set_status(comp, I_CAL_STATUS_COMPLETED);
        e_cal_component_set_percent_complete(comp, 100);
        ICalTime *now = utcNow();
        e_cal_component_set_completed(comp, now);
        g_object_unref(now);
    } else {
        e_cal_component_set_status(comp, I_CAL_STATUS_NEEDSACTION);
        e_cal_component_set_percent_complete(comp, -1);
        e_cal_component_set_completed(comp, nullptr);
    }
}

void setReminderOf(ECalComponent *comp, const QDateTime &at, const QString &title)
{
    e_cal_component_remove_all_alarms(comp);
    if (!at.isValid()) {
        return;
    }
    ECalComponentAlarm *alarm = e_cal_component_alarm_new();
    e_cal_component_alarm_set_action(alarm, E_CAL_COMPONENT_ALARM_DISPLAY);
    ICalTime *when = i_cal_time_new_from_timet_with_zone(time_t(at.toSecsSinceEpoch()), FALSE, i_cal_timezone_get_utc_timezone());
    e_cal_component_alarm_take_trigger(alarm, e_cal_component_alarm_trigger_new_absolute(when));
    g_object_unref(when);
    e_cal_component_alarm_take_description(alarm, e_cal_component_text_new(title.toUtf8().constData(), nullptr));
    e_cal_component_add_alarm(comp, alarm);
    e_cal_component_alarm_free(alarm);
}

void setRepeatOf(ECalComponent *comp, const QString &repeat, ICalTimezone *zone)
{
    const QString rule = ruleFor(repeat);
    if (rule.isEmpty()) {
        e_cal_component_set_rrules(comp, nullptr);
        return;
    }
    GSList *rules = g_slist_prepend(nullptr, i_cal_recurrence_new_from_string(rule.toUtf8().constData()));
    e_cal_component_set_rrules(comp, rules);
    g_slist_free_full(rules, g_object_unref);
    // The rule counts from the start: the due day, else today.
    ECalComponentDateTime *due = e_cal_component_get_due(comp);
    if (due) {
        e_cal_component_set_dtstart(comp, due);
        e_cal_component_datetime_free(due);
    } else {
        ECalComponentDateTime *today = e_cal_component_datetime_new_take(icalTime(QDateTime::currentDateTime(), true, zone), nullptr);
        e_cal_component_set_dtstart(comp, today);
        e_cal_component_set_due(comp, today);
        e_cal_component_datetime_free(today);
    }
}

// A repeating task, ticked, moves on to its next due day instead of
// staying done, as To Do and Reminders do. False if the rule has ended.
bool advance(ECalComponent *comp)
{
    ECalComponentDateTime *due = e_cal_component_get_due(comp);
    GSList *rules = e_cal_component_get_rrules(comp);
    bool moved = false;
    if (due && rules) {
        ICalTime *from = e_cal_component_datetime_get_value(due);
        ICalRecurIterator *it = i_cal_recur_iterator_new(static_cast<ICalRecurrence *>(rules->data), from);
        for (ICalTime *next = it ? i_cal_recur_iterator_next(it) : nullptr; next; next = i_cal_recur_iterator_next(it)) {
            const bool later = !i_cal_time_is_null_time(next) && i_cal_time_compare(next, from) > 0;
            if (i_cal_time_is_null_time(next) || later) {
                if (later) {
                    ECalComponentDateTime *dt = e_cal_component_datetime_new(next, e_cal_component_datetime_get_tzid(due));
                    e_cal_component_set_due(comp, dt);
                    e_cal_component_set_dtstart(comp, dt);
                    e_cal_component_datetime_free(dt);
                    moved = true;
                }
                g_object_unref(next);
                break;
            }
            g_object_unref(next);
        }
        g_clear_object(&it);
    }
    g_slist_free_full(rules, g_object_unref);
    e_cal_component_datetime_free(due);
    return moved;
}

void setStepsOf(ICalComponent *icomp, const QVariantList &steps)
{
    removeProperties(icomp, propertiesOf(icomp, I_CAL_X_PROPERTY, stepName));
    for (const QVariant &v : steps) {
        const QVariantMap m = v.toMap();
        const QString text = m.value(QStringLiteral("text")).toString().simplified();
        if (text.isEmpty()) {
            continue;
        }
        const QString value = (m.value(QStringLiteral("done")).toBool() ? QStringLiteral("[x] ") : QStringLiteral("[ ] ")) + text;
        ICalProperty *p = i_cal_property_new_x(value.toUtf8().constData());
        i_cal_property_set_x_name(p, stepName);
        i_cal_component_take_property(icomp, p);
    }
}

void touch(ECalComponent *comp)
{
    ICalTime *now = utcNow();
    e_cal_component_set_last_modified(comp, now);
    e_cal_component_set_dtstamp(comp, now);
    g_object_unref(now);
}
}

void TasksEngine::change(const QString &key, std::function<void(Task &)> local, std::function<void(ECalComponent *)> edit)
{
    QString listUid, uid;
    if (!splitKey(key, &listUid, &uid)) {
        return;
    }
    ECalClient *c = client(listUid);
    if (!c) {
        return;
    }
    if (Task *t = find(key)) {
        if (local) {
            local(*t);
            if (m_perList.contains(listUid)) {
                for (Task &x : m_perList[listUid]) {
                    if (x.key == key) {
                        local(x);
                    }
                }
            }
            ++m_revision;
            Q_EMIT tasksChanged();
        }
    }
    ICalTimezone *z = zone();
    g_object_ref(c);
    auto error = std::make_shared<QString>();
    run([c, uid, edit, error, z]() {
        ICalComponent *icomp = nullptr;
        GError *err = nullptr;
        if (!e_cal_client_get_object_sync(c, uid.toUtf8().constData(), nullptr, &icomp, nullptr, &err)) {
            *error = str(err ? err->message : "");
            g_clear_error(&err);
            g_object_unref(c);
            return;
        }
        ECalComponent *comp = e_cal_component_new_from_icalcomponent(icomp);
        if (!comp) {
            g_object_unref(c);
            return;
        }
        edit(comp);
        touch(comp);
        e_cal_component_commit_sequence(comp);
        e_cal_client_add_timezone_sync(c, z, nullptr, nullptr);
        if (!e_cal_client_modify_object_sync(c, e_cal_component_get_icalcomponent(comp), E_CAL_OBJ_MOD_ALL, E_CAL_OPERATION_FLAG_NONE, nullptr, &err)) {
            *error = str(err ? err->message : "");
        }
        g_clear_error(&err);
        g_object_unref(comp);
        g_object_unref(c);
    }, [this, listUid, error]() {
        if (!error->isEmpty()) {
            qCWarning(tasksLog) << "couldn't save task:" << *error;
            Q_EMIT notice(tr("The task wasn't saved. %1").arg(*error));
        }
        scheduleReload(listUid);
    });
}

void TasksEngine::addTask(const QString &title, const QString &view, const QVariantMap &extra)
{
    const QString text = title.simplified();
    if (text.isEmpty()) {
        return;
    }
    QString listUid = view;
    bool isList = false;
    for (const List &l : m_lists) {
        isList = isList || l.uid == view;
    }
    if (!isList) {
        listUid = defaultList();
    }
    ECalClient *c = client(listUid);
    if (!c) {
        Q_EMIT notice(tr("That list isn't open yet. Try again in a moment."));
        return;
    }
    QDateTime due = extra.value(QStringLiteral("due")).toDateTime();
    bool allDay = extra.value(QStringLiteral("dueAllDay"), true).toBool();
    if (!due.isValid() && view == QLatin1String("planned")) {
        due = QDateTime(QDate::currentDate(), QTime(0, 0));
        allDay = true;
    }
    const bool important = view == QLatin1String("important") || extra.value(QStringLiteral("important")).toBool();
    const bool myDay = view == QLatin1String("myday") || extra.value(QStringLiteral("myDay")).toBool();
    const QDateTime reminder = extra.value(QStringLiteral("reminder")).toDateTime();
    const QString repeat = extra.value(QStringLiteral("repeat")).toString();
    ICalTimezone *z = zone();
    g_object_ref(c);
    auto newUid = std::make_shared<QString>();
    auto error = std::make_shared<QString>();
    run([=]() {
        ECalComponent *comp = e_cal_component_new();
        e_cal_component_set_new_vtype(comp, E_CAL_COMPONENT_TODO);
        gchar *generated = e_util_generate_uid();
        e_cal_component_set_uid(comp, generated);
        g_free(generated);
        setText(comp, text);
        ICalTime *now = utcNow();
        e_cal_component_set_created(comp, now);
        g_object_unref(now);
        e_cal_component_set_status(comp, I_CAL_STATUS_NEEDSACTION);
        if (important) {
            e_cal_component_set_priority(comp, 1);
        }
        setDueOf(comp, due, allDay, z);
        if (!repeat.isEmpty() && repeat != QLatin1String("none")) {
            setRepeatOf(comp, repeat, z);
        }
        setReminderOf(comp, reminder, text);
        ICalComponent *icomp = e_cal_component_get_icalcomponent(comp);
        if (myDay) {
            e_cal_util_component_set_x_property(icomp, myDayName, QDate::currentDate().toString(Qt::ISODate).toUtf8().constData());
        }
        touch(comp);
        GError *err = nullptr;
        if (due.isValid() && !allDay) {
            e_cal_client_add_timezone_sync(c, z, nullptr, nullptr);
        }
        gchar *uid = nullptr;
        if (e_cal_client_create_object_sync(c, icomp, E_CAL_OPERATION_FLAG_NONE, &uid, nullptr, &err)) {
            *newUid = uid ? str(uid) : str(e_cal_component_get_uid(comp));
        } else {
            *error = str(err ? err->message : "");
        }
        g_free(uid);
        g_clear_error(&err);
        g_object_unref(comp);
        g_object_unref(c);
    }, [this, listUid, newUid, error]() {
        if (!error->isEmpty()) {
            Q_EMIT notice(tr("The task wasn't added. %1").arg(*error));
            return;
        }
        scheduleReload(listUid);
        Q_EMIT added(keyOf(listUid, *newUid));
    });
}

void TasksEngine::setDone(const QString &key, bool done)
{
    const Task *t = find(key);
    const bool repeats = t && t->repeat != QLatin1String("none") && t->due.isValid();
    change(key, [done, repeats](Task &x) {
        if (!repeats || !done) {
            x.done = done;
            x.completed = done ? QDateTime::currentDateTime() : QDateTime();
        }
    }, [done](ECalComponent *comp) {
        if (done && e_cal_component_has_rrules(comp) && advance(comp)) {
            return;
        }
        setDoneOf(comp, done);
    });
}

void TasksEngine::setImportant(const QString &key, bool important)
{
    change(key, [important](Task &x) {
        x.important = important;
    }, [important](ECalComponent *comp) {
        e_cal_component_set_priority(comp, important ? 1 : -1);
    });
}

void TasksEngine::setMyDay(const QString &key, bool on)
{
    const QDate today = QDate::currentDate();
    change(key, [on, today](Task &x) {
        x.myDay = on ? today : QDate();
    }, [on, today](ECalComponent *comp) {
        e_cal_util_component_set_x_property(e_cal_component_get_icalcomponent(comp), myDayName,
                                            on ? today.toString(Qt::ISODate).toUtf8().constData() : nullptr);
    });
}

void TasksEngine::setTitle(const QString &key, const QString &title)
{
    const QString text = title.simplified();
    if (text.isEmpty()) {
        return;
    }
    change(key, [text](Task &x) {
        x.title = text;
    }, [text](ECalComponent *comp) {
        setText(comp, text);
    });
}

void TasksEngine::setNotes(const QString &key, const QString &notes)
{
    change(key, [notes](Task &x) {
        x.notes = notes;
    }, [notes](ECalComponent *comp) {
        setNotesOf(comp, notes);
    });
}

void TasksEngine::setDue(const QString &key, const QDateTime &due, bool allDay)
{
    ICalTimezone *z = zone();
    change(key, [due, allDay](Task &x) {
        x.due = due;
        x.dueAllDay = allDay;
    }, [due, allDay, z](ECalComponent *comp) {
        setDueOf(comp, due, allDay, z);
        if (!due.isValid()) {
            // No repeat without a day to count from.
            e_cal_component_set_rrules(comp, nullptr);
            e_cal_component_set_dtstart(comp, nullptr);
        }
    });
}

void TasksEngine::setReminder(const QString &key, const QDateTime &at)
{
    const Task *t = find(key);
    const QString title = t ? t->title : QString();
    change(key, [at](Task &x) {
        x.reminder = at;
    }, [at, title](ECalComponent *comp) {
        setReminderOf(comp, at, title);
    });
}

void TasksEngine::setRepeat(const QString &key, const QString &repeat)
{
    ICalTimezone *z = zone();
    change(key, [repeat](Task &x) {
        x.repeat = ruleFor(repeat).isEmpty() ? QStringLiteral("none") : repeat;
        if (x.repeat != QLatin1String("none") && !x.due.isValid()) {
            x.due = QDateTime(QDate::currentDate(), QTime(0, 0));
            x.dueAllDay = true;
        }
    }, [repeat, z](ECalComponent *comp) {
        setRepeatOf(comp, repeat, z);
    });
}

void TasksEngine::setSteps(const QString &key, const QVariantList &steps)
{
    change(key, [steps](Task &x) {
        x.steps.clear();
        for (const QVariant &v : steps) {
            const QVariantMap m = v.toMap();
            const QString text = m.value(QStringLiteral("text")).toString().simplified();
            if (!text.isEmpty()) {
                x.steps << qMakePair(m.value(QStringLiteral("done")).toBool(), text);
            }
        }
    }, [steps](ECalComponent *comp) {
        setStepsOf(e_cal_component_get_icalcomponent(comp), steps);
    });
}

void TasksEngine::moveTo(const QString &key, const QString &listUid)
{
    QString from, uid;
    if (!splitKey(key, &from, &uid) || from == listUid) {
        return;
    }
    ECalClient *source = client(from);
    ECalClient *target = client(listUid);
    if (!source || !target) {
        return;
    }
    g_object_ref(source);
    g_object_ref(target);
    auto error = std::make_shared<QString>();
    run([source, target, uid, error]() {
        ICalComponent *icomp = nullptr;
        GError *err = nullptr;
        if (e_cal_client_get_object_sync(source, uid.toUtf8().constData(), nullptr, &icomp, nullptr, &err)) {
            if (e_cal_client_create_object_sync(target, icomp, E_CAL_OPERATION_FLAG_NONE, nullptr, nullptr, &err)) {
                e_cal_client_remove_object_sync(source, uid.toUtf8().constData(), nullptr, E_CAL_OBJ_MOD_ALL, E_CAL_OPERATION_FLAG_NONE, nullptr, nullptr);
            } else {
                *error = str(err ? err->message : "");
            }
            g_object_unref(icomp);
        } else {
            *error = str(err ? err->message : "");
        }
        g_clear_error(&err);
        g_object_unref(source);
        g_object_unref(target);
    }, [this, from, listUid, error]() {
        if (!error->isEmpty()) {
            Q_EMIT notice(tr("The task wasn't moved. %1").arg(*error));
        }
        scheduleReload(from);
        scheduleReload(listUid);
    });
}

void TasksEngine::remove(const QString &key)
{
    QString listUid, uid;
    if (!splitKey(key, &listUid, &uid)) {
        return;
    }
    ECalClient *c = client(listUid);
    if (!c) {
        return;
    }
    if (m_perList.contains(listUid)) {
        auto &list = m_perList[listUid];
        list.erase(std::remove_if(list.begin(), list.end(), [&key](const Task &t) {
            return t.key == key;
        }), list.end());
        flatten();
    }
    g_object_ref(c);
    auto error = std::make_shared<QString>();
    run([c, uid, error]() {
        GError *err = nullptr;
        if (!e_cal_client_remove_object_sync(c, uid.toUtf8().constData(), nullptr, E_CAL_OBJ_MOD_ALL, E_CAL_OPERATION_FLAG_NONE, nullptr, &err)) {
            *error = str(err ? err->message : "");
        }
        g_clear_error(&err);
        g_object_unref(c);
    }, [this, listUid, error]() {
        if (!error->isEmpty()) {
            Q_EMIT notice(tr("The task wasn't deleted. %1").arg(*error));
        }
        scheduleReload(listUid);
    });
}

void TasksEngine::addFile(const QString &key, const QUrl &url)
{
    QFile file(url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (!file.open(QIODevice::ReadOnly)) {
        Q_EMIT notice(tr("The file couldn't be read."));
        return;
    }
    if (file.size() > largestFile) {
        Q_EMIT notice(tr("Files up to 5 MB can go with a task. Put bigger ones in Files and add a link in the note."));
        return;
    }
    const QByteArray data = file.readAll().toBase64();
    const QString name = QFileInfo(file).fileName();
    const QString mime = QMimeDatabase().mimeTypeForFile(QFileInfo(file)).name();
    const qint64 size = file.size();
    change(key, [name, size](Task &x) {
        x.files << qMakePair(name, size);
    }, [data, name, mime](ECalComponent *comp) {
        // The attachment owns its copy and frees it with g_free.
        gchar *copy = g_strndup(data.constData(), gsize(data.size()));
        ICalAttach *attach = i_cal_attach_new_from_data(copy, reinterpret_cast<GFunc>(g_free), nullptr);
        ICalProperty *p = i_cal_property_new_attach(attach);
        g_object_unref(attach);
        i_cal_property_take_parameter(p, i_cal_parameter_new_encoding(I_CAL_ENCODING_BASE64));
        i_cal_property_take_parameter(p, i_cal_parameter_new_value(I_CAL_VALUE_BINARY));
        i_cal_property_take_parameter(p, i_cal_parameter_new_fmttype(mime.toUtf8().constData()));
        i_cal_property_set_parameter_from_string(p, "X-FILENAME", name.toUtf8().constData());
        i_cal_component_take_property(e_cal_component_get_icalcomponent(comp), p);
    });
}

void TasksEngine::removeFile(const QString &key, int index)
{
    change(key, [index](Task &x) {
        if (index >= 0 && index < x.files.size()) {
            x.files.removeAt(index);
        }
    }, [index](ECalComponent *comp) {
        ICalComponent *icomp = e_cal_component_get_icalcomponent(comp);
        QList<ICalProperty *> props = propertiesOf(icomp, I_CAL_ATTACH_PROPERTY);
        for (int i = 0; i < props.size(); ++i) {
            if (i == index) {
                i_cal_component_remove_property(icomp, props[i]);
            }
            g_object_unref(props[i]);
        }
    });
}

void TasksEngine::openFile(const QString &key, int index)
{
    QString listUid, uid;
    ECalClient *c = splitKey(key, &listUid, &uid) ? client(listUid) : nullptr;
    if (!c) {
        return;
    }
    g_object_ref(c);
    auto path = std::make_shared<QString>();
    run([c, uid, index, path]() {
        ICalComponent *icomp = nullptr;
        if (e_cal_client_get_object_sync(c, uid.toUtf8().constData(), nullptr, &icomp, nullptr, nullptr)) {
            const QList<ICalProperty *> props = propertiesOf(icomp, I_CAL_ATTACH_PROPERTY);
            for (int i = 0; i < props.size(); ++i) {
                if (i == index) {
                    ICalAttach *attach = i_cal_property_get_attach(props[i]);
                    if (attach && i_cal_attach_get_is_url(attach)) {
                        *path = str(i_cal_attach_get_url(attach));
                    } else if (attach) {
                        const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/task-files/") + uid;
                        QDir().mkpath(dir);
                        QString name = fileNameOf(props[i], attach);
                        name.replace(QRegularExpression(QStringLiteral("[/\\\\]")), QStringLiteral("_"));
                        QFile out(dir + QLatin1Char('/') + name);
                        if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                            out.write(QByteArray::fromBase64(QByteArray(i_cal_attach_get_data(attach))));
                            *path = QUrl::fromLocalFile(out.fileName()).toString();
                        }
                    }
                    g_clear_object(&attach);
                }
                g_object_unref(props[i]);
            }
            g_object_unref(icomp);
        }
        g_object_unref(c);
    }, [this, path]() {
        if (path->isEmpty()) {
            Q_EMIT notice(tr("The file couldn't be opened."));
            return;
        }
        QDesktopServices::openUrl(QUrl(*path));
    });
}

void TasksEngine::addList(const QString &name)
{
    const QString n = name.simplified();
    if (n.isEmpty() || !m_registry) {
        return;
    }
    ESource *source = e_source_new(nullptr, nullptr, nullptr);
    e_source_set_parent(source, "local-stub");
    e_source_set_display_name(source, n.toUtf8().constData());
    auto *ext = e_source_get_extension(source, E_SOURCE_EXTENSION_TASK_LIST);
    e_source_backend_set_backend_name(E_SOURCE_BACKEND(ext), "local");
    e_source_selectable_set_selected(E_SOURCE_SELECTABLE(ext), TRUE);
    ESourceRegistry *registry = m_registry;
    g_object_ref(registry);
    auto error = std::make_shared<QString>();
    run([registry, source, error]() {
        GList *list = g_list_append(nullptr, source);
        GError *err = nullptr;
        if (!e_source_registry_create_sources_sync(registry, list, nullptr, &err)) {
            *error = str(err ? err->message : "");
        }
        g_clear_error(&err);
        g_list_free(list);
        g_object_unref(source);
        g_object_unref(registry);
    }, [this, error]() {
        if (!error->isEmpty()) {
            Q_EMIT notice(tr("The list wasn't made. %1").arg(*error));
        }
    });
}

void TasksEngine::renameList(const QString &uid, const QString &name)
{
    if (ESource *source = m_registry ? e_source_registry_ref_source(m_registry, uid.toUtf8().constData()) : nullptr) {
        e_source_set_display_name(source, name.simplified().toUtf8().constData());
        e_source_write(source, nullptr, nullptr, nullptr);
        g_object_unref(source);
    }
}

void TasksEngine::removeList(const QString &uid)
{
    if (ESource *source = m_registry ? e_source_registry_ref_source(m_registry, uid.toUtf8().constData()) : nullptr) {
        if (e_source_get_removable(source)) {
            e_source_remove(source, nullptr, nullptr, nullptr);
        } else {
            // An account's own list, or this PC's: hidden instead.
            e_source_set_enabled(source, FALSE);
            e_source_write(source, nullptr, nullptr, nullptr);
        }
        g_object_unref(source);
    }
}

void TasksEngine::refresh()
{
    for (ECalClient *c : std::as_const(m_clients)) {
        if (c) {
            startRefresh(E_CLIENT(c));
        }
    }
}

void TasksEngine::startRefresh(EClient *client)
{
    if (m_syncing++ == 0) {
        m_syncHold.start();
        Q_EMIT syncingChanged();
    }
    e_client_refresh(client, m_cancel, &TasksEngine::onRefreshed, nullptr);
}

void TasksEngine::onRefreshed(GObject *object, GAsyncResult *result, gpointer)
{
    GError *error = nullptr;
    e_client_refresh_finish(E_CLIENT(object), result, &error);
    const bool cancelled = g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_clear_error(&error);
    if (cancelled) {
        return;
    }
    TasksEngine *self = instance();
    if (--self->m_syncing == 0) {
        Q_EMIT self->syncingChanged();
    }
}

