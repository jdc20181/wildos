#pragma once

#include <QDateTime>
#include <QFile>
#include <QRegularExpression>
#include <QTimeZone>

#include <cmath>
#include <optional>

// Sunrise and sunset for the automatic theme, worked out offline from the
// computer's time zone. The zone's main city (from the tz database's own
// table) stands in for the computer's position: close enough for when it
// gets dark, and it needs no location permission or network.
namespace Sun
{

struct Place {
    double latitude = 0;
    double longitude = 0;
};

// "+5130-00008" or "+513030-0000731": degrees and minutes (and seconds)
// for latitude, then longitude.
inline std::optional<Place> parseCoordinates(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("^([+-])(\\d{2})(\\d{2})(\\d{2})?([+-])(\\d{3})(\\d{2})(\\d{2})?$"));
    const QRegularExpressionMatch m = pattern.match(text);
    if (!m.hasMatch()) {
        return std::nullopt;
    }
    const auto part = [&m](int sign, int deg, int min, int sec) {
        const double value = m.captured(deg).toDouble() + m.captured(min).toDouble() / 60 + m.captured(sec).toDouble() / 3600;
        return m.captured(sign) == QLatin1String("-") ? -value : value;
    };
    return Place{part(1, 2, 3, 4), part(5, 6, 7, 8)};
}

inline std::optional<Place> placeOfZone(const QByteArray &zone)
{
    for (const QString &table : {QStringLiteral("/usr/share/zoneinfo/zone1970.tab"), QStringLiteral("/usr/share/zoneinfo/zone.tab")}) {
        QFile file(table);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        while (!file.atEnd()) {
            const QByteArray line = file.readLine().trimmed();
            if (line.startsWith('#')) {
                continue;
            }
            const QList<QByteArray> fields = line.split('\t');
            if (fields.size() >= 3 && fields.at(2) == zone) {
                return parseCoordinates(QString::fromLatin1(fields.at(1)));
            }
        }
    }
    return std::nullopt;
}

struct Day {
    // Both invalid in a polar day or night; then alwaysLight says which.
    QDateTime sunrise;
    QDateTime sunset;
    bool alwaysLight = false;
};

// The sunrise equation (as NOAA and Wikipedia give it), accurate to a few
// minutes, which is all a theme change needs.
inline Day dayAt(const QDate &date, const Place &place)
{
    constexpr double rad = M_PI / 180;
    const double julianNoon = date.toJulianDay();
    const double n = std::round(julianNoon - 2451545.0 + 0.0008);
    const double meanNoon = n - place.longitude / 360;
    const double anomaly = std::fmod(357.5291 + 0.98560028 * meanNoon, 360);
    const double centre = 1.9148 * std::sin(anomaly * rad) + 0.02 * std::sin(2 * anomaly * rad) + 0.0003 * std::sin(3 * anomaly * rad);
    const double eclipticLongitude = std::fmod(anomaly + centre + 180 + 102.9372, 360);
    const double transit = 2451545.0 + meanNoon + 0.0053 * std::sin(anomaly * rad) - 0.0069 * std::sin(2 * eclipticLongitude * rad);
    const double declination = std::asin(std::sin(eclipticLongitude * rad) * std::sin(23.4397 * rad));
    const double cosHourAngle = (std::sin(-0.833 * rad) - std::sin(place.latitude * rad) * std::sin(declination))
        / (std::cos(place.latitude * rad) * std::cos(declination));

    Day day;
    if (cosHourAngle > 1 || cosHourAngle < -1) {
        day.alwaysLight = cosHourAngle < -1;
        return day;
    }
    const double halfDay = std::acos(cosHourAngle) / rad / 360;
    const auto toTime = [](double julian) {
        return QDateTime::fromMSecsSinceEpoch(qint64((julian - 2440587.5) * 86400000.0), QTimeZone::UTC).toLocalTime();
    };
    day.sunrise = toTime(transit - halfDay);
    day.sunset = toTime(transit + halfDay);
    return day;
}

// Whether the light theme suits this moment. Without a known place it is
// light from 07:00 to 19:00.
inline bool isLight(const QDateTime &now)
{
    const std::optional<Place> place = placeOfZone(QTimeZone::systemTimeZoneId());
    if (!place) {
        const int hour = now.time().hour();
        return hour >= 7 && hour < 19;
    }
    const Day day = dayAt(now.date(), *place);
    if (!day.sunrise.isValid()) {
        return day.alwaysLight;
    }
    return now >= day.sunrise && now < day.sunset;
}

}
