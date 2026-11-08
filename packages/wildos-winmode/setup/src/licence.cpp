#include "licence.h"

#include <QRegularExpression>
#include <QtEndian>

namespace
{
const QString keyAlphabet = QStringLiteral("BCDFGHJKMNPQRTVWXY2346789");
constexpr int headerSize = 36;
constexpr int keyOffset = headerSize + 20;
constexpr int keyLength = 29;
}

namespace Licence
{
Msdm parseMsdm(const QByteArray &table)
{
    Msdm result;
    if (table.size() < keyOffset + keyLength) {
        result.why = QStringLiteral("table too short");
        return result;
    }
    if (!table.startsWith("MSDM")) {
        result.why = QStringLiteral("not an MSDM table");
        return result;
    }
    const auto *bytes = reinterpret_cast<const uchar *>(table.constData());
    const quint32 length = qFromLittleEndian<quint32>(bytes + 4);
    if (length < quint32(keyOffset + keyLength) || length > quint32(table.size())) {
        result.why = QStringLiteral("length field does not match");
        return result;
    }
    // Every ACPI table sums to zero over its stated length; a table that
    // does not was damaged or made up.
    quint8 sum = 0;
    for (quint32 i = 0; i < length; ++i) {
        sum += bytes[i];
    }
    if (sum != 0) {
        result.why = QStringLiteral("checksum does not match");
        return result;
    }
    const quint32 version = qFromLittleEndian<quint32>(bytes + headerSize);
    const quint32 dataType = qFromLittleEndian<quint32>(bytes + headerSize + 8);
    const quint32 dataLength = qFromLittleEndian<quint32>(bytes + headerSize + 16);
    if (version != 1 || dataType != 1 || dataLength != quint32(keyLength)) {
        result.why = QStringLiteral("not a product key structure");
        return result;
    }
    const QString key = QString::fromLatin1(table.mid(keyOffset, keyLength));
    if (!keyShapeValid(key)) {
        result.why = QStringLiteral("key has the wrong shape");
        return result;
    }
    result.valid = true;
    result.key = key;
    result.oemId = QString::fromLatin1(table.mid(10, 6)).trimmed();
    return result;
}

bool keyShapeValid(const QString &key)
{
    static const QRegularExpression shape(QStringLiteral("^([%1]{5}-){4}[%1]{5}$").arg(keyAlphabet));
    // N may appear once, anywhere: Windows 8 keys encode a position with it.
    return shape.match(key).hasMatch() && key.count(QLatin1Char('N')) <= 1;
}

QString normaliseKey(const QString &typed)
{
    QString compact;
    for (const QChar c : typed.toUpper()) {
        if (c.isLetterOrNumber()) {
            compact += c;
        }
    }
    if (compact.size() != 25) {
        return compact;
    }
    QStringList groups;
    for (int i = 0; i < 25; i += 5) {
        groups << compact.mid(i, 5);
    }
    return groups.join(QLatin1Char('-'));
}

QString keyEnd(const QString &key)
{
    return key.right(5);
}

QString editionFrom(const QString &editionId, const QString &keyDescription)
{
    const QString id = editionId.trimmed();
    if (id.startsWith(QLatin1String("Core"))) {
        return QStringLiteral("home");
    }
    if (id.startsWith(QLatin1String("Professional")) || id.startsWith(QLatin1String("Education")) || id.startsWith(QLatin1String("Enterprise"))) {
        return QStringLiteral("pro");
    }
    // "[4.0] Core OEM:DM", "[4.0] Professional OEM:DM"
    static const QRegularExpression described(QStringLiteral("^\\[[0-9.]+\\]\\s+(\\S+)"));
    const auto match = described.match(keyDescription.trimmed());
    if (match.hasMatch() && match.captured(1) != id) {
        return editionFrom(match.captured(1));
    }
    return {};
}

QString editionName(const QString &edition)
{
    if (edition == QLatin1String("home")) {
        return QStringLiteral("Windows 11 Home");
    }
    if (edition == QLatin1String("pro")) {
        return QStringLiteral("Windows 11 Pro");
    }
    return QStringLiteral("Windows 11");
}
}
