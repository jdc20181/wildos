#include "schemes.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>

namespace
{
// Homebrew first: it is the default and leads the list in Settings.
const QStringList order = {
    QStringLiteral("Homebrew"), QStringLiteral("WildOS"), QStringLiteral("Campbell"),
    QStringLiteral("OneHalfDark"), QStringLiteral("SolarizedDark"), QStringLiteral("OneHalfLight"),
};

QColor readColour(QSettings &file, const QString &group, const QColor &fallback)
{
    const QStringList parts = file.value(group + QStringLiteral("/Color")).toStringList();
    if (parts.size() < 3) {
        return fallback;
    }
    return QColor(parts[0].toInt(), parts[1].toInt(), parts[2].toInt());
}

QList<Scheme> load()
{
    QList<Scheme> list;
    const QDir dir(Schemes::directory());
    const QStringList files = dir.entryList({QStringLiteral("*.colorscheme")}, QDir::Files, QDir::Name);
    for (const QString &name : files) {
        Scheme s;
        s.path = dir.absoluteFilePath(name);
        s.id = QFileInfo(name).completeBaseName();
        QSettings file(s.path, QSettings::IniFormat);
        // QSettings files the keys of an INI [General] group at the top level.
        s.name = file.value(QStringLiteral("Description"), s.id).toString();
        s.background = readColour(file, QStringLiteral("Background"), Qt::black);
        s.foreground = readColour(file, QStringLiteral("Foreground"), Qt::white);
        s.foregroundBold = readColour(file, QStringLiteral("ForegroundIntense"), s.foreground);
        for (int i = 0; i < 16; ++i) {
            const QString group = QStringLiteral("Color%1%2").arg(i % 8).arg(i >= 8 ? QStringLiteral("Intense") : QString());
            s.colours.append(readColour(file, group, s.foreground));
        }
        list.append(s);
    }
    std::stable_sort(list.begin(), list.end(), [](const Scheme &a, const Scheme &b) {
        const int ia = order.indexOf(a.id);
        const int ib = order.indexOf(b.id);
        return (ia < 0 ? 1000 : ia) < (ib < 0 ? 1000 : ib);
    });
    if (list.isEmpty()) {
        Scheme fallback;
        fallback.id = QStringLiteral("Homebrew");
        fallback.name = QStringLiteral("Homebrew");
        fallback.background = Qt::black;
        fallback.foreground = QColor(0x28, 0xfe, 0x14);
        fallback.foregroundBold = fallback.foreground;
        for (int i = 0; i < 16; ++i) {
            fallback.colours.append(fallback.foreground);
        }
        list.append(fallback);
    }
    return list;
}
}

QString Schemes::directory()
{
    // A build run from its tree uses the schemes beside the sources.
    const QString installed = QStringLiteral(WILDOS_TERMINAL_DATADIR "/schemes");
    if (QFileInfo::exists(installed)) {
        return installed;
    }
    return QStringLiteral(WILDOS_TERMINAL_SOURCEDIR "/schemes");
}

const QList<Scheme> &Schemes::all()
{
    static const QList<Scheme> list = load();
    return list;
}

const Scheme &Schemes::find(const QString &id)
{
    for (const Scheme &s : all()) {
        if (s.id == id) {
            return s;
        }
    }
    return all().first();
}

QVariantList Schemes::toVariant()
{
    QVariantList out;
    for (const Scheme &s : all()) {
        QVariantList colours;
        for (const QColor &c : s.colours) {
            colours.append(c);
        }
        out.append(QVariantMap{
            {QStringLiteral("id"), s.id},
            {QStringLiteral("name"), s.name},
            {QStringLiteral("background"), s.background},
            {QStringLiteral("foreground"), s.foreground},
            {QStringLiteral("foregroundBold"), s.foregroundBold},
            {QStringLiteral("colours"), colours},
        });
    }
    return out;
}
