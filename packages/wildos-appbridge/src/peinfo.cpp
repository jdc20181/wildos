#include "peinfo.h"

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QList>

#include <cstring>

namespace
{
constexpr quint32 rtIcon = 3;
constexpr quint32 rtGroupIcon = 14;
constexpr quint32 rtVersion = 16;

// A bounds-checked view of the mapped file, so a damaged or hostile file
// can only make reading fail, never read past the end.
struct View {
    const uchar *data = nullptr;
    qint64 size = 0;

    bool has(qint64 offset, qint64 length) const
    {
        return offset >= 0 && length >= 0 && offset <= size && length <= size - offset;
    }
    quint16 u16(qint64 offset) const
    {
        return has(offset, 2) ? quint16(data[offset] | data[offset + 1] << 8) : 0;
    }
    quint32 u32(qint64 offset) const
    {
        return has(offset, 4) ? quint32(data[offset]) | quint32(data[offset + 1]) << 8 | quint32(data[offset + 2]) << 16 | quint32(data[offset + 3]) << 24 : 0;
    }
};

struct Section {
    quint32 virtualAddress;
    quint32 virtualSize;
    quint32 rawOffset;
    quint32 rawSize;
};

struct Image {
    View file;
    QList<Section> sections;
    qint64 resourceOffset = -1;
    quint32 resourceRva = 0;

    qint64 offsetOf(quint32 rva) const
    {
        for (const Section &s : sections) {
            const quint32 extent = std::max(s.virtualSize, s.rawSize);
            if (rva >= s.virtualAddress && rva < s.virtualAddress + extent) {
                return qint64(s.rawOffset) + (rva - s.virtualAddress);
            }
        }
        return -1;
    }
};

struct Entry {
    quint32 id;
    bool isDirectory;
    quint32 target;
};

QList<Entry> directoryEntries(const Image &image, qint64 directory)
{
    QList<Entry> entries;
    const View &f = image.file;
    if (!f.has(directory, 16)) {
        return entries;
    }
    const int count = f.u16(directory + 12) + f.u16(directory + 14);
    for (int i = 0; i < count && i < 4096; ++i) {
        const qint64 at = directory + 16 + i * 8;
        if (!f.has(at, 8)) {
            break;
        }
        const quint32 name = f.u32(at);
        const quint32 target = f.u32(at + 4);
        entries.append({name & 0x80000000 ? 0xffffffff : name, bool(target & 0x80000000), target & 0x7fffffff});
    }
    return entries;
}

// Resources are three levels deep: type, then name or id, then language.
// Any language will do, so the first is taken.
QByteArray resource(const Image &image, quint32 type, quint32 id, bool anyId)
{
    if (image.resourceOffset < 0) {
        return {};
    }
    const View &f = image.file;
    for (const Entry &typeEntry : directoryEntries(image, image.resourceOffset)) {
        if (typeEntry.id != type || !typeEntry.isDirectory) {
            continue;
        }
        for (const Entry &nameEntry : directoryEntries(image, image.resourceOffset + typeEntry.target)) {
            if ((!anyId && nameEntry.id != id) || !nameEntry.isDirectory) {
                continue;
            }
            const auto languages = directoryEntries(image, image.resourceOffset + nameEntry.target);
            if (languages.isEmpty() || languages.first().isDirectory) {
                continue;
            }
            const qint64 dataEntry = image.resourceOffset + languages.first().target;
            const qint64 offset = image.offsetOf(f.u32(dataEntry));
            const quint32 size = f.u32(dataEntry + 4);
            if (offset < 0 || !f.has(offset, size)) {
                return {};
            }
            return QByteArray(reinterpret_cast<const char *>(f.data + offset), size);
        }
    }
    return {};
}

QList<QByteArray> groupIcons(const Image &image)
{
    QList<QByteArray> groups;
    if (image.resourceOffset < 0) {
        return groups;
    }
    for (const Entry &typeEntry : directoryEntries(image, image.resourceOffset)) {
        if (typeEntry.id != rtGroupIcon || !typeEntry.isDirectory) {
            continue;
        }
        for (const Entry &nameEntry : directoryEntries(image, image.resourceOffset + typeEntry.target)) {
            const QByteArray group = resource(image, rtGroupIcon, nameEntry.id, nameEntry.id == 0xffffffff);
            if (!group.isEmpty()) {
                groups.append(group);
            }
        }
    }
    return groups;
}

// The first icon group is the one Explorer shows. Its largest picture is
// taken, either a PNG as stored or a bitmap wrapped back into an .ico so
// Qt's own reader can decode it.
QImage bestIcon(const Image &image)
{
    const QList<QByteArray> groups = groupIcons(image);
    if (groups.isEmpty()) {
        return {};
    }
    const QByteArray &group = groups.first();
    View g{reinterpret_cast<const uchar *>(group.constData()), group.size()};
    const int count = g.u16(4);
    int best = -1;
    int bestScore = -1;
    for (int i = 0; i < count; ++i) {
        const qint64 at = 6 + i * 14;
        if (!g.has(at, 14)) {
            break;
        }
        const int width = g.data[at] == 0 ? 256 : g.data[at];
        const int bits = g.u16(at + 6);
        const int score = width * 64 + bits;
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }
    if (best < 0) {
        return {};
    }
    const qint64 at = 6 + best * 14;
    const QByteArray picture = resource(image, rtIcon, g.u16(at + 12), false);
    if (picture.isEmpty()) {
        return {};
    }
    if (picture.startsWith("\x89PNG")) {
        return QImage::fromData(picture, "PNG");
    }
    QByteArray ico;
    QBuffer buffer(&ico);
    buffer.open(QIODevice::WriteOnly);
    const auto put16 = [&buffer](quint16 v) {
        const char b[2] = {char(v & 0xff), char(v >> 8)};
        buffer.write(b, 2);
    };
    const auto put32 = [&buffer](quint32 v) {
        const char b[4] = {char(v & 0xff), char((v >> 8) & 0xff), char((v >> 16) & 0xff), char(v >> 24)};
        buffer.write(b, 4);
    };
    put16(0);
    put16(1);
    put16(1);
    buffer.write(group.constData() + at, 8);
    put32(quint32(picture.size()));
    put32(22);
    buffer.write(picture);
    buffer.close();
    return QImage::fromData(ico, "ICO");
}

// VS_VERSIONINFO is a tree of blocks: length, value length, type, a UTF-16
// key, padding to four bytes, the value, then child blocks. The strings
// wanted live under StringFileInfo, one table per language.
void readVersionBlock(const View &v, qint64 offset, qint64 end, int depth, QHash<QString, QString> &strings)
{
    while (offset + 6 < end && depth < 6) {
        const quint16 length = v.u16(offset);
        const quint16 valueLength = v.u16(offset + 2);
        const quint16 type = v.u16(offset + 4);
        if (length < 6 || !v.has(offset, length)) {
            return;
        }
        const qint64 blockEnd = offset + length;
        qint64 cursor = offset + 6;
        QString key;
        while (cursor + 2 <= blockEnd) {
            const quint16 c = v.u16(cursor);
            cursor += 2;
            if (c == 0) {
                break;
            }
            key.append(QChar(c));
        }
        cursor = (cursor + 3) & ~qint64(3);
        if (depth == 3) {
            // A string's value length counts UTF-16 characters, not bytes.
            QString value;
            for (int i = 0; i < valueLength && cursor + 2 <= blockEnd; ++i, cursor += 2) {
                const quint16 c = v.u16(cursor);
                if (c == 0) {
                    break;
                }
                value.append(QChar(c));
            }
            if (!strings.contains(key)) {
                strings.insert(key, value.trimmed());
            }
        } else {
            const qint64 valueBytes = type == 1 ? qint64(valueLength) * 2 : valueLength;
            cursor = (cursor + valueBytes + 3) & ~qint64(3);
            if (depth == 0 || key == QLatin1String("StringFileInfo") || depth == 2) {
                readVersionBlock(v, cursor, blockEnd, depth + 1, strings);
            }
        }
        offset = (blockEnd + 3) & ~qint64(3);
    }
}

bool parse(const View &f, Image &image, PeInfo &info)
{
    if (!f.has(0, 64) || f.data[0] != 'M' || f.data[1] != 'Z') {
        return false;
    }
    const qint64 pe = f.u32(0x3c);
    if (!f.has(pe, 24) || std::memcmp(f.data + pe, "PE\0\0", 4) != 0) {
        return false;
    }
    info.kind = PeInfo::Exe;
    const int sectionCount = f.u16(pe + 6);
    const int optionalSize = f.u16(pe + 20);
    const qint64 optional = pe + 24;
    const quint16 magic = f.u16(optional);
    info.is64Bit = magic == 0x20b;
    const qint64 directories = optional + (info.is64Bit ? 112 : 96);
    const quint32 directoryCount = f.u32(optional + (info.is64Bit ? 108 : 92));
    const auto directory = [&](quint32 index) -> std::pair<quint32, quint32> {
        if (index >= directoryCount) {
            return {0, 0};
        }
        return {f.u32(directories + index * 8), f.u32(directories + index * 8 + 4)};
    };

    const qint64 sectionTable = optional + optionalSize;
    for (int i = 0; i < sectionCount && i < 96; ++i) {
        const qint64 at = sectionTable + i * 40;
        if (!f.has(at, 40)) {
            break;
        }
        image.sections.append({f.u32(at + 12), f.u32(at + 8), f.u32(at + 20), f.u32(at + 16)});
    }

    // The security entry holds a file offset rather than an address.
    const auto [certOffset, certSize] = directory(4);
    info.signedFile = certOffset != 0 && certSize > 8 && f.has(certOffset, certSize);
    info.dotNet = directory(14).first != 0;

    const auto [resourceRva, resourceSize] = directory(2);
    if (resourceRva != 0 && resourceSize != 0) {
        image.resourceRva = resourceRva;
        image.resourceOffset = image.offsetOf(resourceRva);
    }
    return true;
}
}

PeInfo PeInfo::read(const QString &path)
{
    PeInfo info;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() < 64) {
        return info;
    }
    uchar *mapped = file.map(0, file.size());
    if (!mapped) {
        return info;
    }
    const View f{mapped, file.size()};
    // Windows Installer packages are OLE compound files.
    if (f.has(0, 8) && std::memcmp(mapped, "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1", 8) == 0) {
        info.kind = Msi;
        file.unmap(mapped);
        return info;
    }
    Image image;
    image.file = f;
    if (parse(f, image, info)) {
        const QByteArray version = resource(image, rtVersion, 1, true);
        if (!version.isEmpty()) {
            QHash<QString, QString> strings;
            const View v{reinterpret_cast<const uchar *>(version.constData()), version.size()};
            readVersionBlock(v, 0, version.size(), 0, strings);
            info.productName = strings.value(QStringLiteral("ProductName"));
            info.companyName = strings.value(QStringLiteral("CompanyName"));
            info.fileDescription = strings.value(QStringLiteral("FileDescription"));
            info.productVersion = strings.value(QStringLiteral("ProductVersion"));
        }
        info.icon = bestIcon(image);
    }
    file.unmap(mapped);
    return info;
}

QImage PeInfo::readIcon(const QString &path)
{
    return read(path).icon;
}
