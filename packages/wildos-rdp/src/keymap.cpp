#include "keymap.h"

#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QString>

namespace keymap
{
namespace
{
constexpr uint32_t ext = 0x100;

// Linux key codes 1 to 88 are the PC keyboard's set 1 make codes; the rest
// are listed one by one.
uint32_t beyondSetOne(uint32_t evdev)
{
    switch (evdev) {
    case 89: return 0x73;         // RO
    case 92: return 0x79;         // Henkan
    case 93: return 0x70;         // Katakana/Hiragana
    case 94: return 0x7B;         // Muhenkan
    case 96: return 0x1C | ext;   // keypad Enter
    case 97: return 0x1D | ext;   // right Ctrl
    case 98: return 0x35 | ext;   // keypad /
    case 99: return 0x37 | ext;   // Print Screen
    case 100: return 0x38 | ext;  // right Alt (AltGr)
    case 102: return 0x47 | ext;  // Home
    case 103: return 0x48 | ext;  // Up
    case 104: return 0x49 | ext;  // Page Up
    case 105: return 0x4B | ext;  // Left
    case 106: return 0x4D | ext;  // Right
    case 107: return 0x4F | ext;  // End
    case 108: return 0x50 | ext;  // Down
    case 109: return 0x51 | ext;  // Page Down
    case 110: return 0x52 | ext;  // Insert
    case 111: return 0x53 | ext;  // Delete
    case 113: return 0x20 | ext;  // Mute
    case 114: return 0x2E | ext;  // Volume down
    case 115: return 0x30 | ext;  // Volume up
    case 117: return 0x59;        // keypad =
    case 121: return 0x7E;        // keypad , (Brazil)
    case 122: return 0x72;        // Hangul
    case 123: return 0x71;        // Hanja
    case 124: return 0x7D;        // Yen
    case 125: return 0x5B | ext;  // left Windows
    case 126: return 0x5C | ext;  // right Windows
    case 127: return 0x5D | ext;  // Menu
    case 140: return 0x21 | ext;  // Calculator
    case 155: return 0x6C | ext;  // Mail
    case 158: return 0x6A | ext;  // Back
    case 159: return 0x69 | ext;  // Forward
    case 163: return 0x19 | ext;  // Next track
    case 164: return 0x22 | ext;  // Play/Pause
    case 165: return 0x10 | ext;  // Previous track
    case 166: return 0x24 | ext;  // Stop
    case 172: return 0x32 | ext;  // Home page
    case 217: return 0x65 | ext;  // Search
    default: break;
    }
    if (evdev >= 183 && evdev <= 193) {
        return 0x64 + (evdev - 183); // F13 to F23
    }
    if (evdev == 194) {
        return 0x76; // F24
    }
    return 0;
}

struct Layout {
    const char *xkb;
    uint32_t klid;
};

// Plasma's layout names (xkb) to Windows' keyboard layout identifiers.
constexpr Layout layouts[] = {
    {"us", 0x00000409},       {"us(intl)", 0x00020409},    {"us(alt-intl)", 0x00020409}, {"us(dvorak)", 0x00010409},
    {"gb", 0x00000809},       {"ie", 0x00001809},          {"de", 0x00000407},           {"at", 0x00000C07},
    {"ch", 0x00000807},       {"ch(fr)", 0x0000100C},      {"fr", 0x0000040C},           {"be", 0x0000080C},
    {"ca", 0x00001009},       {"ca(multix)", 0x00011009},  {"es", 0x0000040A},           {"latam", 0x0000080A},
    {"pt", 0x00000816},       {"br", 0x00000416},          {"it", 0x00000410},           {"nl", 0x00000413},
    {"se", 0x0000041D},       {"no", 0x00000414},          {"dk", 0x00000406},           {"fi", 0x0000040B},
    {"is", 0x0000040F},       {"pl", 0x00000415},          {"cz", 0x00000405},           {"sk", 0x0000041B},
    {"hu", 0x0000040E},       {"ru", 0x00000419},          {"ua", 0x00000422},           {"gr", 0x00000408},
    {"tr", 0x0000041F},       {"jp", 0x00000411},          {"kr", 0x00000412},           {"il", 0x0000040D},
    {"ara", 0x00000401},      {"ro", 0x00010418},          {"hr", 0x0000041A},           {"si", 0x00000424},
    {"rs", 0x00000C1A},       {"bg", 0x00030402},          {"lt", 0x00010427},           {"lv", 0x00020426},
    {"ee", 0x00000425},
};

uint32_t lookup(const QString &layout, const QString &variant)
{
    const QString full = variant.isEmpty() ? layout : layout + u'(' + variant + u')';
    for (const Layout &l : layouts) {
        if (full == QLatin1String(l.xkb)) {
            return l.klid;
        }
    }
    for (const Layout &l : layouts) {
        if (layout == QLatin1String(l.xkb)) {
            return l.klid;
        }
    }
    return 0;
}

QString first(const QString &list)
{
    return list.section(u',', 0, 0).trimmed();
}
}

uint32_t rdpScancode(uint32_t evdev)
{
    if (evdev >= 1 && evdev <= 88 && evdev != 84 && evdev != 85) {
        return evdev;
    }
    return beyondSetOne(evdev);
}

uint32_t windowsLayout()
{
    const QString kxkbrc = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, QStringLiteral("kxkbrc"));
    if (!kxkbrc.isEmpty()) {
        QSettings s(kxkbrc, QSettings::IniFormat);
        s.beginGroup(QStringLiteral("Layout"));
        const QString layout = first(s.value(QStringLiteral("LayoutList")).toString());
        const QString variant = first(s.value(QStringLiteral("VariantList")).toString());
        if (!layout.isEmpty()) {
            if (const uint32_t id = lookup(layout, variant)) {
                return id;
            }
        }
    }
    // Plasma follows the system layout until the person picks one.
    QFile system(QStringLiteral("/etc/default/keyboard"));
    if (system.open(QIODevice::ReadOnly)) {
        QString layout, variant;
        for (const QByteArray &raw : system.readAll().split('\n')) {
            const QString line = QString::fromUtf8(raw).trimmed();
            const QString value = line.section(u'=', 1).remove(u'"').trimmed();
            if (line.startsWith(QLatin1String("XKBLAYOUT="))) {
                layout = first(value);
            } else if (line.startsWith(QLatin1String("XKBVARIANT="))) {
                variant = first(value);
            }
        }
        if (const uint32_t id = lookup(layout, variant)) {
            return id;
        }
    }
    const QString lang = qEnvironmentVariable("LANG");
    const QString territory = lang.section(u'_', 1).section(u'.', 0, 0).toLower();
    if (territory == QLatin1String("us")) {
        return 0x00000409;
    }
    return lookup(territory, {});
}
}
