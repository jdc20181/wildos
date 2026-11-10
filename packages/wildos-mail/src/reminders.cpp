#include "reminders.h"
#include "formats.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QLocale>
#include <QProcess>
#include <QScopeGuard>
#include <QTimeZone>

namespace
{
const QString service = QStringLiteral("org.freedesktop.Notifications");
const QString path = QStringLiteral("/org/freedesktop/Notifications");

QString str(const gchar *s)
{
    return s ? QString::fromUtf8(s) : QString();
}

// "in 5 minutes", "now", "started 10 minutes ago".
QString whenText(qint64 start)
{
    const qint64 minutes = (start - QDateTime::currentSecsSinceEpoch() + 30) / 60;
    if (minutes == 0) {
        return QObject::tr("now");
    }
    if (minutes > 0) {
        if (minutes < 60) {
            return minutes == 1 ? QObject::tr("in 1 minute") : QObject::tr("in %1 minutes").arg(minutes);
        }
        if (minutes < 24 * 60) {
            return minutes < 90 ? QObject::tr("in 1 hour") : QObject::tr("in %1 hours").arg((minutes + 30) / 60);
        }
        return minutes < 36 * 60 ? QObject::tr("tomorrow") : QObject::tr("in %1 days").arg((minutes + 12 * 60) / (24 * 60));
    }
    return minutes == -1 ? QObject::tr("started 1 minute ago") : QObject::tr("started %1 minutes ago").arg(-minutes);
}
}

Reminders::Reminders(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(service, path, service, QStringLiteral("ActionInvoked"), this, SLOT(onAction(uint, QString)));
    bus.connect(service, path, service, QStringLiteral("NotificationClosed"), this, SLOT(onClosed(uint, uint)));
}

Reminders::~Reminders()
{
    for (EReminderData *rd : std::as_const(m_shown)) {
        e_reminder_data_free(rd);
    }
    g_clear_object(&m_watcher);
    g_clear_object(&m_registry);
}

bool Reminders::start()
{
    GError *error = nullptr;
    m_registry = e_source_registry_new_sync(nullptr, &error);
    if (!m_registry) {
        qWarning("Calendar reminders: the calendar service didn't start: %s", error ? error->message : "");
        g_clear_error(&error);
        return false;
    }
    m_watcher = e_reminder_watcher_new(m_registry);
    ICalTimezone *zone = i_cal_timezone_get_builtin_timezone(QTimeZone::systemTimeZoneId().constData());
    if (zone) {
        e_reminder_watcher_set_default_zone(m_watcher, zone);
    }
    g_signal_connect(m_watcher, "triggered", G_CALLBACK(&Reminders::onTriggered), this);
    return true;
}

void Reminders::onTriggered(EReminderWatcher *, const GSList *reminders, gboolean, gpointer self)
{
    qInfo("Calendar reminders: %u due", g_slist_length(const_cast<GSList *>(reminders)));
    for (const GSList *l = reminders; l; l = l->next) {
        static_cast<Reminders *>(self)->show(static_cast<const EReminderData *>(l->data));
    }
}

void Reminders::show(const EReminderData *data)
{
    ECalComponent *comp = e_reminder_data_get_component(data);
    const ECalComponentAlarmInstance *instance = e_reminder_data_get_instance(data);
    if (!comp || !instance) {
        return;
    }
    ICalComponent *icomp = e_cal_component_get_icalcomponent(comp);
    QString title = str(i_cal_component_get_summary(icomp));
    if (title.isEmpty()) {
        title = tr("(No title)");
    }
    const QString location = str(i_cal_component_get_location(icomp));
    const qint64 start = e_cal_component_alarm_instance_get_occur_start(instance);
    const qint64 end = e_cal_component_alarm_instance_get_occur_end(instance);
    const QDateTime s = QDateTime::fromSecsSinceEpoch(start);
    const QDateTime e = QDateTime::fromSecsSinceEpoch(end);
    ICalTime *dtstart = i_cal_component_get_dtstart(icomp);
    const bool allDay = dtstart && i_cal_time_is_date(dtstart);
    g_clear_object(&dtstart);

    QString body = allDay ? tr("All day") : tr("%1 to %2").arg(Formats::time(s), Formats::time(e));
    if (s.date() != QDate::currentDate()) {
        body = Formats::shortDayAndMonth(s.date()) + QStringLiteral(", ") + body;
    }
    if (!location.isEmpty()) {
        body += QStringLiteral(", ") + location;
    }

    QDBusMessage call = QDBusMessage::createMethodCall(service, path, service, QStringLiteral("Notify"));
    const QStringList actions = {QStringLiteral("default"), tr("Open"),
                                 QStringLiteral("snooze"), tr("Snooze for 5 minutes"),
                                 QStringLiteral("dismiss"), tr("Dismiss")};
    const QVariantMap hints = {
        {QStringLiteral("desktop-entry"), QStringLiteral("org.wildos.calendar")},
        {QStringLiteral("category"), QStringLiteral("x-wildos.calendar.reminder")},
        {QStringLiteral("resident"), true},
        {QStringLiteral("urgency"), QVariant::fromValue<uchar>(1)},
    };
    // "Calendar  in 5 minutes" over the event, as the board has it.
    const QString heading = allDay ? title : tr("%1, %2").arg(title, whenText(start));
    call << tr("Calendar") << uint(0) << QStringLiteral("org.wildos.calendar") << heading << body.toHtmlEscaped() << actions << hints << int(0);
    EReminderData *copy = e_reminder_data_copy(data);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, copy]() {
        QDBusPendingReply<uint> reply = *watcher;
        if (reply.isError()) {
            e_reminder_data_free(copy);
        } else {
            m_shown.insert(reply.value(), copy);
        }
        watcher->deleteLater();
    });
}

void Reminders::onAction(uint id, const QString &action)
{
    EReminderData *rd = m_shown.take(id);
    if (!rd) {
        return;
    }
    // Resident notifications stay after a button; this one is done.
    QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(service, path, service, QStringLiteral("CloseNotification")) << id);
    const auto cleanup = qScopeGuard([rd]() {
        e_reminder_data_free(rd);
    });
    if (action == QLatin1String("snooze")) {
        e_reminder_watcher_snooze(m_watcher, rd, QDateTime::currentSecsSinceEpoch() + 5 * 60);
    } else if (action == QLatin1String("dismiss")) {
        e_reminder_watcher_dismiss_sync(m_watcher, rd, nullptr, nullptr);
    } else if (action == QLatin1String("default")) {
        ECalComponent *comp = e_reminder_data_get_component(rd);
        const ECalComponentAlarmInstance *instance = e_reminder_data_get_instance(rd);
        const QString uid = str(e_cal_component_get_uid(comp));
        const qint64 start = instance ? e_cal_component_alarm_instance_get_occur_start(instance) : 0;
        // Opening it counts as having seen it.
        e_reminder_watcher_dismiss_sync(m_watcher, rd, nullptr, nullptr);
        QProcess::startDetached(QStringLiteral("wildos-calendar"),
                                {QStringLiteral("--event"), str(e_reminder_data_get_source_uid(rd)) + QChar(0x1f) + uid,
                                 QStringLiteral("--day"), QDateTime::fromSecsSinceEpoch(start).date().toString(Qt::ISODate)});
    }
}

void Reminders::onClosed(uint id, uint reason)
{
    // 2 is closed by the person; a reminder closed without snoozing is
    // dismissed, so it doesn't come back.
    if (EReminderData *rd = m_shown.take(id)) {
        if (reason == 2) {
            e_reminder_watcher_dismiss_sync(m_watcher, rd, nullptr, nullptr);
        }
        e_reminder_data_free(rd);
    }
}
