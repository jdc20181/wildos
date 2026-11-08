#include "language.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCollator>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QSet>

namespace
{
const QString helper = QStringLiteral("/usr/libexec/wildos/settings-locale");
const QString fallbackFormat = QStringLiteral("en_GB.UTF-8");
const QString fallbackLanguage = QStringLiteral("en_GB");

// The separate formats KDE's region module can set; choosing a regional
// format here sets them all, as Windows does.
const QStringList categories = {QStringLiteral("LC_NUMERIC"), QStringLiteral("LC_TIME"), QStringLiteral("LC_MONETARY"),
                                QStringLiteral("LC_MEASUREMENT"), QStringLiteral("LC_COLLATE"), QStringLiteral("LC_CTYPE"),
                                QStringLiteral("LC_PAPER"), QStringLiteral("LC_ADDRESS"), QStringLiteral("LC_NAME"),
                                QStringLiteral("LC_TELEPHONE")};

// "en_GB.utf8", "en_GB.UTF-8" and "en_GB" are the same locale.
QString canonical(QString locale)
{
    locale = locale.trimmed();
    const int at = locale.indexOf(QLatin1Char('@'));
    const QString modifier = at >= 0 ? locale.mid(at) : QString();
    if (at >= 0) {
        locale.truncate(at);
    }
    const QString base = locale.section(QLatin1Char('.'), 0, 0);
    if (base.isEmpty() || base == QLatin1String("C") || base == QLatin1String("POSIX")) {
        return {};
    }
    return base + QStringLiteral(".UTF-8") + modifier;
}

QString firstLanguage(const QString &list)
{
    for (const QString &entry : list.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        const QString code = entry.section(QLatin1Char('.'), 0, 0);
        if (!code.isEmpty() && code != QLatin1String("C")) {
            return code;
        }
    }
    return {};
}

QString capitalised(QString text)
{
    if (!text.isEmpty()) {
        text[0] = text[0].toUpper();
    }
    return text;
}

QStringList supported()
{
    QStringList list;
    QFile file(QStringLiteral("/usr/share/i18n/SUPPORTED"));
    if (file.open(QIODevice::ReadOnly)) {
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            const QString name = line.section(QLatin1Char(' '), 0, 0);
            if (line.endsWith(QLatin1String(" UTF-8")) && name.endsWith(QLatin1String(".UTF-8")) && !name.contains(QLatin1Char('@'))) {
                list << name;
            }
        }
    }
    return list;
}

QSet<QString> generated()
{
    QProcess process;
    process.start(QStringLiteral("locale"), {QStringLiteral("-a")});
    process.waitForFinished(5000);
    QSet<QString> set;
    const auto lines = QString::fromUtf8(process.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.contains(QLatin1String("utf8"), Qt::CaseInsensitive) || line.contains(QLatin1String("UTF-8"), Qt::CaseInsensitive)) {
            set.insert(canonical(line));
        }
    }
    return set;
}
}

Language::Language(QObject *parent)
    : QObject(parent)
{
    // What this session started with, which is what apps use until the
    // next sign-in; wildos.sh gives English sessions UK English text.
    m_sessionLanguage = firstLanguage(qEnvironmentVariable("LANGUAGE"));
    m_sessionFormat = canonical(qEnvironmentVariable("LANG"));
    if (m_sessionFormat.isEmpty()) {
        m_sessionFormat = fallbackFormat;
    }
    if (m_sessionLanguage.isEmpty() || m_sessionLanguage == QLatin1String("en")) {
        m_sessionLanguage = m_sessionFormat.startsWith(QLatin1String("en_")) ? fallbackLanguage : m_sessionFormat.section(QLatin1Char('_'), 0, 0);
    }
    read();
}

void Language::read()
{
    auto config = KSharedConfig::openConfig(QStringLiteral("plasma-localerc"), KConfig::NoGlobals);
    config->reparseConfiguration();
    const KConfigGroup formats(config, QStringLiteral("Formats"));
    const KConfigGroup translations(config, QStringLiteral("Translations"));
    m_language = firstLanguage(translations.readEntry("LANGUAGE", QString()));
    if (m_language.isEmpty()) {
        m_language = m_sessionLanguage;
    }
    m_format = canonical(formats.readEntry("LANG", QString()));
    if (m_format.isEmpty()) {
        m_format = m_sessionFormat;
    }
    m_detailed = false;
    for (const QString &key : categories) {
        const QString value = canonical(formats.readEntry(key, QString()));
        if (!value.isEmpty() && value != m_format) {
            m_detailed = true;
        }
    }
    Q_EMIT changed();
}

bool Language::needsSignOut() const
{
    return m_language != m_sessionLanguage || m_format != m_sessionFormat;
}

QVariantList Language::languages() const
{
    // A language counts when the desktop shell itself is translated into it.
    QSet<QString> codes = {fallbackLanguage};
    const auto dirs = QDir(QStringLiteral("/usr/share/locale")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &code : dirs) {
        if (!code.contains(QLatin1Char('@')) && code != QLatin1String("en")
            && QFile::exists(QStringLiteral("/usr/share/locale/") + code + QStringLiteral("/LC_MESSAGES/plasmashell.mo"))) {
            codes.insert(code);
        }
    }
    QVariantList list;
    for (const QString &code : std::as_const(codes)) {
        const QLocale locale(code);
        if (locale.language() == QLocale::C || code.size() > 5) {
            continue;
        }
        // Qt names UK English "British English", which would then read
        // "British English (United Kingdom)" with the territory.
        QString text = locale.language() == QLocale::English ? QStringLiteral("English") : capitalised(locale.nativeLanguageName());
        if (code.contains(QLatin1Char('_'))) {
            text += QStringLiteral(" (") + locale.nativeTerritoryName() + QLatin1Char(')');
        }
        QString english = QLocale::languageToString(locale.language());
        if (code.contains(QLatin1Char('_'))) {
            english += QStringLiteral(" (") + QLocale::territoryToString(locale.territory()) + QLatin1Char(')');
        }
        list << QVariantMap{{QStringLiteral("value"), code}, {QStringLiteral("text"), text}, {QStringLiteral("english"), english}};
    }
    QCollator collator(QLocale(QStringLiteral("en_GB")));
    std::sort(list.begin(), list.end(), [&collator](const QVariant &a, const QVariant &b) {
        const QVariantMap x = a.toMap();
        const QVariantMap y = b.toMap();
        if ((x.value(QStringLiteral("value")) == fallbackLanguage) != (y.value(QStringLiteral("value")) == fallbackLanguage)) {
            return x.value(QStringLiteral("value")) == fallbackLanguage;
        }
        return collator.compare(x.value(QStringLiteral("english")).toString(), y.value(QStringLiteral("english")).toString()) < 0;
    });
    return list;
}

QVariantList Language::formats() const
{
    QStringList names = supported();
    if (names.isEmpty()) {
        names = generated().values();
    }
    QVariantList list;
    QSet<QString> seen;
    for (const QString &name : std::as_const(names)) {
        const QLocale locale(name.section(QLatin1Char('.'), 0, 0));
        if (locale.language() == QLocale::C) {
            continue;
        }
        const QString text = QLocale::languageToString(locale.language()) + QStringLiteral(" (") + QLocale::territoryToString(locale.territory()) + QLatin1Char(')');
        if (seen.contains(text)) {
            continue;
        }
        seen.insert(text);
        list << QVariantMap{{QStringLiteral("value"), name}, {QStringLiteral("text"), text}};
    }
    QCollator collator(QLocale(QStringLiteral("en_GB")));
    std::sort(list.begin(), list.end(), [&collator](const QVariant &a, const QVariant &b) {
        const QVariantMap x = a.toMap();
        const QVariantMap y = b.toMap();
        if ((x.value(QStringLiteral("value")) == fallbackFormat) != (y.value(QStringLiteral("value")) == fallbackFormat)) {
            return x.value(QStringLiteral("value")) == fallbackFormat;
        }
        return collator.compare(x.value(QStringLiteral("text")).toString(), y.value(QStringLiteral("text")).toString()) < 0;
    });
    return list;
}

QVariantMap Language::examples(const QString &format) const
{
    const QLocale locale(format.section(QLatin1Char('.'), 0, 0));
    const QDateTime now = QDateTime::currentDateTime();
    QString measurement;
    switch (locale.measurementSystem()) {
    case QLocale::ImperialUSSystem:
        measurement = QStringLiteral("Imperial: inches, feet and pounds");
        break;
    case QLocale::ImperialUKSystem:
        measurement = QStringLiteral("Metric, with miles and pints");
        break;
    default:
        measurement = QStringLiteral("Metric: centimetres, metres and kilograms");
        break;
    }
    return {
        {QStringLiteral("shortDate"), locale.toString(now.date(), QLocale::ShortFormat)},
        {QStringLiteral("longDate"), locale.toString(now.date(), QLocale::LongFormat)},
        {QStringLiteral("time"), locale.toString(now.time(), QLocale::ShortFormat)},
        {QStringLiteral("number"), locale.toString(1234567.89, 'f', 2)},
        {QStringLiteral("currency"), locale.toCurrencyString(1234.5)},
        {QStringLiteral("firstDay"), QLocale(QStringLiteral("en_GB")).dayName(locale.firstDayOfWeek())},
        {QStringLiteral("measurement"), measurement},
    };
}

void Language::ensure(const QStringList &locales, const std::function<void()> &then)
{
    const QSet<QString> have = generated();
    QStringList missing;
    for (const QString &l : locales) {
        if (!have.contains(canonical(l))) {
            missing << l;
        }
    }
    if (missing.isEmpty()) {
        then();
        return;
    }
    m_busy = true;
    Q_EMIT busyChanged();
    auto process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, then](int code, QProcess::ExitStatus status) {
        const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed().section(QLatin1Char('\n'), -1);
        process->deleteLater();
        m_busy = false;
        Q_EMIT busyChanged();
        if (status == QProcess::NormalExit && code == 0) {
            then();
        } else {
            Q_EMIT failed(code == 126 || code == 127 ? QStringLiteral("An administrator's password is needed to add a language to this computer.")
                                                     : error.isEmpty() ? QStringLiteral("The language could not be set up.") : error);
            Q_EMIT changed();
        }
    });
    process->start(QStringLiteral("pkexec"), QStringList{helper} + missing);
}

void Language::setLanguage(const QString &code)
{
    if (code.isEmpty() || code == m_language) {
        return;
    }
    // Translations are read through LANGUAGE alone; glibc only needs a
    // UTF-8 locale for them, which the regional format already is.
    auto config = KSharedConfig::openConfig(QStringLiteral("plasma-localerc"), KConfig::NoGlobals);
    KConfigGroup translations(config, QStringLiteral("Translations"));
    // UK English stands behind every other language, so anything a
    // translation lacks reads in Wild OS's own words.
    const QString list = code == fallbackLanguage ? QStringLiteral("en_GB:en") : code + QStringLiteral(":en_GB:en");
    translations.writeEntry("LANGUAGE", list, KConfig::Notify);
    config->sync();
    read();
}

void Language::setFormat(const QString &locale)
{
    const QString chosen = canonical(locale);
    if (chosen.isEmpty() || chosen == m_format || m_busy) {
        return;
    }
    ensure({chosen}, [this, chosen] {
        auto config = KSharedConfig::openConfig(QStringLiteral("plasma-localerc"), KConfig::NoGlobals);
        KConfigGroup formats(config, QStringLiteral("Formats"));
        formats.writeEntry("LANG", chosen, KConfig::Notify);
        for (const QString &key : categories) {
            formats.deleteEntry(key, KConfig::Notify);
        }
        // Written out so Wild OS's day-first, 24-hour default for US English
        // sessions (wildos.sh) gives way to a format chosen here.
        formats.writeEntry("LC_TIME", chosen, KConfig::Notify);
        config->sync();
        read();
    });
}

void Language::signOut()
{
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.Shutdown"), QStringLiteral("/Shutdown"), QStringLiteral("org.kde.Shutdown"),
                                                  QStringLiteral("logout"));
    QDBusConnection::sessionBus().asyncCall(message);
}
