#include "settings.h"
#include "paths.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSettings>

#include <algorithm>

namespace
{
Settings *s_instance = nullptr;

const QString kDrives = QStringLiteral("drives/list");
const QString kHome = QStringLiteral("include/home");
const QString kMail = QStringLiteral("include/mail");
const QString kSettings = QStringLiteral("include/settings");
const QString kWindows = QStringLiteral("include/windows");
const QString kExcludes = QStringLiteral("include/excludes");
const QString kWithin = QStringLiteral("keep/within");
const QString kDaily = QStringLiteral("keep/daily");
const QString kWeekly = QStringLiteral("keep/weekly");
const QString kMonthly = QStringLiteral("keep/monthly");
const QString kScheduled = QStringLiteral("schedule/on");
const QString kTime = QStringLiteral("schedule/time");
const QString kDays = QStringLiteral("schedule/days");
const QString kNotify = QStringLiteral("notify/finished");
}

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings(Paths::settingsFile(), QSettings::IniFormat))
{
    s_instance = this;
}

Settings::~Settings()
{
    delete m_settings;
}

Settings *Settings::instance()
{
    if (!s_instance) {
        new Settings(QCoreApplication::instance());
    }
    return s_instance;
}

Settings *Settings::create(QQmlEngine *, QJSEngine *)
{
    Settings *settings = instance();
    QQmlEngine::setObjectOwnership(settings, QQmlEngine::CppOwnership);
    return settings;
}

void Settings::reload()
{
    m_settings->sync();
    // The scheduled run writes only the per drive notes.
    Q_EMIT drivesChanged();
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    m_settings->setValue(key, value);
    m_settings->sync();
}

QJsonArray Settings::driveList() const
{
    return QJsonDocument::fromJson(m_settings->value(kDrives).toString().toUtf8()).array();
}

QVariantList Settings::drives() const
{
    return driveList().toVariantList();
}

bool Settings::configured() const
{
    return !driveList().isEmpty();
}

QString Settings::defaultDrive() const
{
    const QJsonArray list = driveList();
    for (const QJsonValue &v : list) {
        if (v.toObject().value(QStringLiteral("isDefault")).toBool()) {
            return v.toObject().value(QStringLiteral("uuid")).toString();
        }
    }
    return list.isEmpty() ? QString() : list.first().toObject().value(QStringLiteral("uuid")).toString();
}

QJsonObject Settings::drive(const QString &uuid) const
{
    for (const QJsonValue &v : driveList()) {
        if (v.toObject().value(QStringLiteral("uuid")).toString() == uuid) {
            return v.toObject();
        }
    }
    return {};
}

void Settings::setDrives(const QJsonArray &drives)
{
    setValue(kDrives, QString::fromUtf8(QJsonDocument(drives).toJson(QJsonDocument::Compact)));
    Q_EMIT drivesChanged();
}

void Settings::addDrive(const QString &uuid, const QString &name, const QString &kind)
{
    QJsonArray list = driveList();
    for (qsizetype i = 0; i < list.size(); ++i) {
        if (list.at(i).toObject().value(QStringLiteral("uuid")).toString() == uuid) {
            list.removeAt(i);
            break;
        }
    }
    list.append(QJsonObject{
        {QStringLiteral("uuid"), uuid},
        {QStringLiteral("name"), name},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("isDefault"), list.isEmpty()},
    });
    setDrives(list);
}

void Settings::removeDrive(const QString &uuid)
{
    QJsonArray list = driveList();
    bool wasDefault = false;
    for (qsizetype i = 0; i < list.size(); ++i) {
        if (list.at(i).toObject().value(QStringLiteral("uuid")).toString() == uuid) {
            wasDefault = list.at(i).toObject().value(QStringLiteral("isDefault")).toBool();
            list.removeAt(i);
            break;
        }
    }
    if (wasDefault && !list.isEmpty()) {
        QJsonObject first = list.first().toObject();
        first.insert(QStringLiteral("isDefault"), true);
        list.replace(0, first);
    }
    setDrives(list);
}

void Settings::makeDefault(const QString &uuid)
{
    QJsonArray list = driveList();
    for (qsizetype i = 0; i < list.size(); ++i) {
        QJsonObject d = list.at(i).toObject();
        d.insert(QStringLiteral("isDefault"), d.value(QStringLiteral("uuid")).toString() == uuid);
        list.replace(i, d);
    }
    setDrives(list);
}

void Settings::setDriveValue(const QString &uuid, const QString &key, const QJsonValue &value)
{
    m_settings->sync();
    QJsonArray list = driveList();
    for (qsizetype i = 0; i < list.size(); ++i) {
        QJsonObject d = list.at(i).toObject();
        if (d.value(QStringLiteral("uuid")).toString() == uuid) {
            d.insert(key, value);
            list.replace(i, d);
        }
    }
    setDrives(list);
}

bool Settings::includeHome() const
{
    return m_settings->value(kHome, true).toBool();
}
bool Settings::includeMail() const
{
    return m_settings->value(kMail, true).toBool();
}
bool Settings::includeSettings() const
{
    return m_settings->value(kSettings, true).toBool();
}
// Off until asked for: the virtual PC's disk is tens of gigabytes.
bool Settings::includeWindows() const
{
    return m_settings->value(kWindows, false).toBool();
}
void Settings::setIncludeHome(bool on)
{
    setValue(kHome, on);
    Q_EMIT includeChanged();
}
void Settings::setIncludeMail(bool on)
{
    setValue(kMail, on);
    Q_EMIT includeChanged();
}
void Settings::setIncludeSettings(bool on)
{
    setValue(kSettings, on);
    Q_EMIT includeChanged();
}
void Settings::setIncludeWindows(bool on)
{
    setValue(kWindows, on);
    Q_EMIT includeChanged();
}

QStringList Settings::excludes() const
{
    return m_settings->value(kExcludes).toStringList();
}

void Settings::addExclude(const QString &path)
{
    const QString clean = QDir::cleanPath(path);
    if (clean.isEmpty() || clean == QStringLiteral("/")) {
        return;
    }
    QStringList list = excludes();
    if (list.contains(clean)) {
        return;
    }
    list.append(clean);
    list.sort();
    setValue(kExcludes, list);
    Q_EMIT includeChanged();
}

void Settings::removeExclude(const QString &path)
{
    QStringList list = excludes();
    if (list.removeAll(path) > 0) {
        setValue(kExcludes, list);
        Q_EMIT includeChanged();
    }
}

QString Settings::keepWithin() const
{
    return m_settings->value(kWithin, QStringLiteral("2d")).toString();
}
int Settings::keepDaily() const
{
    return m_settings->value(kDaily, 14).toInt();
}
int Settings::keepWeekly() const
{
    return m_settings->value(kWeekly, 13).toInt();
}
int Settings::keepMonthly() const
{
    return m_settings->value(kMonthly, 12).toInt();
}
void Settings::setKeepWithin(const QString &value)
{
    setValue(kWithin, value);
    Q_EMIT keepChanged();
}
void Settings::setKeepDaily(int value)
{
    setValue(kDaily, value);
    Q_EMIT keepChanged();
}
void Settings::setKeepWeekly(int value)
{
    setValue(kWeekly, value);
    Q_EMIT keepChanged();
}
void Settings::setKeepMonthly(int value)
{
    setValue(kMonthly, value);
    Q_EMIT keepChanged();
}

QStringList Settings::keepArguments() const
{
    QStringList args;
    // The newest backup always stays, whatever the choices, so a drive is
    // never tidied down to nothing.
    args << QStringLiteral("--keep-last") << QStringLiteral("1");
    const QString within = keepWithin();
    if (!within.isEmpty() && within != QStringLiteral("0")) {
        args << QStringLiteral("--keep-within") << within;
    }
    // Keep "forever" is -1 here; restic spells it "unlimited".
    const auto add = [&args](const char *option, int n) {
        if (n != 0) {
            args << QString::fromLatin1(option) << (n < 0 ? QStringLiteral("unlimited") : QString::number(n));
        }
    };
    add("--keep-daily", keepDaily());
    add("--keep-weekly", keepWeekly());
    add("--keep-monthly", keepMonthly());
    return args;
}

bool Settings::scheduled() const
{
    return m_settings->value(kScheduled, true).toBool();
}
int Settings::time() const
{
    return std::clamp(m_settings->value(kTime, 9 * 60).toInt(), 0, 24 * 60 - 1);
}
QList<int> Settings::dayList() const
{
    const QVariant stored = m_settings->value(kDays);
    QList<int> out;
    if (!stored.isValid()) {
        return {1, 2, 3, 4, 5, 6, 7};
    }
    for (const QString &s : stored.toString().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const int d = s.toInt();
        if (d >= 1 && d <= 7 && !out.contains(d)) {
            out.append(d);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}
QVariantList Settings::days() const
{
    QVariantList out;
    for (int d : dayList()) {
        out.append(d);
    }
    return out;
}
void Settings::setScheduled(bool on)
{
    setValue(kScheduled, on);
    Q_EMIT scheduleChanged();
}
void Settings::setTime(int minutes)
{
    setValue(kTime, std::clamp(minutes, 0, 24 * 60 - 1));
    Q_EMIT scheduleChanged();
}
void Settings::setDays(const QVariantList &days)
{
    QStringList parts;
    QList<int> sorted;
    for (const QVariant &v : days) {
        const int d = v.toInt();
        if (d >= 1 && d <= 7 && !sorted.contains(d)) {
            sorted.append(d);
        }
    }
    std::sort(sorted.begin(), sorted.end());
    for (int d : sorted) {
        parts.append(QString::number(d));
    }
    // Stored as text so an empty choice stays empty instead of reading
    // back as "never set".
    setValue(kDays, parts.join(QLatin1Char(',')));
    Q_EMIT scheduleChanged();
}

QDateTime Settings::nextRun(const QDateTime &from) const
{
    const QList<int> wanted = dayList();
    if (!scheduled() || wanted.isEmpty()) {
        return {};
    }
    const QTime at = QTime(0, 0).addSecs(time() * 60);
    for (int i = 0; i <= 7; ++i) {
        const QDate day = from.date().addDays(i);
        const QDateTime candidate(day, at);
        if (wanted.contains(day.dayOfWeek()) && candidate > from) {
            return candidate;
        }
    }
    return {};
}

QDateTime Settings::previousRun(const QDateTime &from) const
{
    const QList<int> wanted = dayList();
    if (!scheduled() || wanted.isEmpty()) {
        return {};
    }
    const QTime at = QTime(0, 0).addSecs(time() * 60);
    for (int i = 0; i <= 7; ++i) {
        const QDate day = from.date().addDays(-i);
        const QDateTime candidate(day, at);
        if (wanted.contains(day.dayOfWeek()) && candidate <= from) {
            return candidate;
        }
    }
    return {};
}

bool Settings::passwordShown() const
{
    return m_settings->value(QStringLiteral("setup/passwordShown"), false).toBool();
}
void Settings::setPasswordShown(bool shown)
{
    setValue(QStringLiteral("setup/passwordShown"), shown);
}

bool Settings::notifyFinished() const
{
    return m_settings->value(kNotify, true).toBool();
}
void Settings::setNotifyFinished(bool on)
{
    setValue(kNotify, on);
    Q_EMIT notifyChanged();
}
