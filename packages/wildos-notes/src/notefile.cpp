#include "notefile.h"

#include <QRegularExpression>
#include <QStringDecoder>

namespace
{
const QRegularExpression &titlePattern()
{
    static const QRegularExpression re(QStringLiteral(R"(^#[ \t]+(.*?)(?:[ \t]+#+)?[ \t]*$)"));
    return re;
}

const QRegularExpression &keyPattern()
{
    static const QRegularExpression re(QStringLiteral(R"(^[A-Za-z_][A-Za-z0-9_-]*[ \t]*:)"));
    return re;
}

bool isBlank(QStringView line)
{
    for (const QChar c : line) {
        if (c != u' ' && c != u'\t') {
            return false;
        }
    }
    return true;
}

bool isFence(QStringView line)
{
    const QStringView t = line.trimmed();
    return t.startsWith(u"```") || t.startsWith(u"~~~");
}

// The YAML block counts only if it reads as YAML: a horizontal rule at the
// top of a note followed by another further down is not front matter.
bool looksLikeYaml(const QStringList &inner)
{
    for (const QString &line : inner) {
        if (isBlank(line) || line.startsWith(u'#')) {
            continue;
        }
        return keyPattern().match(line).hasMatch();
    }
    return true;
}

int findKey(const QStringList &inner, const QStringList &keys)
{
    for (int i = 0; i < inner.size(); ++i) {
        const int colon = inner[i].indexOf(u':');
        if (colon <= 0 || !keyPattern().match(inner[i]).hasMatch()) {
            continue;
        }
        const QString name = inner[i].left(colon).trimmed();
        for (const QString &key : keys) {
            if (name.compare(key, Qt::CaseInsensitive) == 0) {
                return i;
            }
        }
    }
    return -1;
}

// "colour" is what Notes writes; a file written by hand may say "color".
QStringList spellings(const QString &key)
{
    if (key == u"colour") {
        return {QStringLiteral("colour"), QStringLiteral("color")};
    }
    return {key};
}
}

namespace NoteFile
{
NoteParts parse(const QByteArray &bytes)
{
    NoteParts parts;
    QByteArray data = bytes;
    if (data.startsWith("\xEF\xBB\xBF")) {
        parts.bom = data.left(3);
        data.remove(0, 3);
    }
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::ConvertInitialBom);
    QString text = decoder.decode(data);
    parts.valid = !decoder.hasError();

    const qsizetype breaks = text.count(u'\n');
    if (breaks > 0 && text.count(QStringLiteral("\r\n")) == breaks) {
        parts.crlf = true;
        text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    }

    qsizetype pos = 0;
    if (text.startsWith(QStringLiteral("---\n"))) {
        qsizetype lineStart = 4;
        QStringList inner;
        while (lineStart <= text.size()) {
            qsizetype lineEnd = text.indexOf(u'\n', lineStart);
            if (lineEnd < 0) {
                lineEnd = text.size();
            }
            const QString line = text.mid(lineStart, lineEnd - lineStart);
            const QString fence = QStringView(line).trimmed().toString();
            if (fence == u"---" || fence == u"...") {
                if (looksLikeYaml(inner)) {
                    pos = lineEnd < text.size() ? lineEnd + 1 : lineEnd;
                    parts.frontMatter = text.left(pos);
                }
                break;
            }
            inner.append(line);
            if (lineEnd >= text.size()) {
                break;
            }
            lineStart = lineEnd + 1;
        }
    }
    const QString rest = text.mid(pos);

    qsizetype at = 0;
    while (at < rest.size()) {
        const qsizetype nl = rest.indexOf(u'\n', at);
        if (nl < 0 || !isBlank(QStringView(rest).sliced(at, nl - at))) {
            break;
        }
        at = nl + 1;
    }
    qsizetype lineEnd = rest.indexOf(u'\n', at);
    if (lineEnd < 0) {
        lineEnd = rest.size();
    }
    const QString candidate = rest.mid(at, lineEnd - at);
    const auto match = titlePattern().match(candidate);
    if (at < rest.size() && match.hasMatch() && !match.captured(1).trimmed().isEmpty()) {
        parts.lead = rest.left(at);
        parts.titleLine = candidate;
        const QString after = rest.mid(lineEnd);
        qsizetype g = 0;
        if (after.startsWith(u'\n')) {
            g = 1;
            while (g < after.size()) {
                const qsizetype nl = after.indexOf(u'\n', g);
                if (nl < 0 || !isBlank(QStringView(after).sliced(g, nl - g))) {
                    break;
                }
                g = nl + 1;
            }
        }
        parts.gap = after.left(g);
        parts.body = after.mid(g);
    } else {
        parts.body = rest;
    }
    return parts;
}

QByteArray serialize(const NoteParts &parts)
{
    QString gap = parts.gap;
    if (gap.isEmpty() && !parts.titleLine.isEmpty()) {
        // A title written by Notes ends its line, and the text starts after
        // a blank line, as any Markdown editor would write it.
        if (!parts.body.isEmpty()) {
            gap = QStringLiteral("\n\n");
        } else if (parts.addedTitle) {
            gap = QStringLiteral("\n");
        }
    }
    QString text = parts.frontMatter + parts.lead + parts.titleLine + gap + parts.body;
    if (parts.crlf) {
        text.replace(u'\n', QStringLiteral("\r\n"));
    }
    return parts.bom + text.toUtf8();
}

QString headingText(const QString &titleLine)
{
    const auto match = titlePattern().match(titleLine);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString title(const NoteParts &parts, const QString &fileBaseName)
{
    const QString heading = headingText(parts.titleLine);
    return heading.isEmpty() ? fileBaseName : heading;
}

void setTitle(NoteParts &parts, const QString &title)
{
    QString t = title;
    t.replace(u'\n', u' ').replace(u'\r', u' ');
    t = t.trimmed();
    if (t.isEmpty()) {
        parts.titleLine.clear();
        parts.lead.clear();
        parts.gap.clear();
        return;
    }
    if (headingText(parts.titleLine) == t) {
        return;
    }
    if (parts.titleLine.isEmpty()) {
        parts.gap.clear();
        parts.addedTitle = true;
    }
    parts.titleLine = QStringLiteral("# ") + t;
}

QString frontMatterValue(const QString &frontMatter, const QString &key)
{
    if (frontMatter.isEmpty()) {
        return {};
    }
    const QStringList lines = frontMatter.split(u'\n');
    const int i = findKey(lines, spellings(key));
    if (i < 0) {
        return {};
    }
    QString value = lines[i].mid(lines[i].indexOf(u':') + 1).trimmed();
    if (value.size() >= 2 && (value.startsWith(u'"') || value.startsWith(u'\'')) && value.endsWith(value.front())) {
        value = value.mid(1, value.size() - 2);
    }
    return value;
}

bool pinned(const NoteParts &parts)
{
    const QString v = frontMatterValue(parts.frontMatter, QStringLiteral("pinned")).toLower();
    return v == u"true" || v == u"yes" || v == u"on";
}

QString colour(const NoteParts &parts)
{
    return frontMatterValue(parts.frontMatter, QStringLiteral("colour")).toLower();
}

QString setFrontMatterValue(const QString &frontMatter, const QString &key, const QString &value)
{
    QString open = QStringLiteral("---");
    QString close = QStringLiteral("---");
    bool trailingBreak = true;
    QStringList inner;
    if (!frontMatter.isEmpty()) {
        QString s = frontMatter;
        trailingBreak = s.endsWith(u'\n');
        if (trailingBreak) {
            s.chop(1);
        }
        QStringList lines = s.split(u'\n');
        open = lines.takeFirst();
        close = lines.takeLast();
        inner = lines;
    }
    const int i = findKey(inner, spellings(key));
    if (value.isEmpty()) {
        if (i < 0) {
            return frontMatter;
        }
        inner.removeAt(i);
    } else if (i >= 0) {
        const QString name = inner[i].left(inner[i].indexOf(u':')).trimmed();
        inner[i] = name + QStringLiteral(": ") + value;
    } else {
        // Pinned goes above the colour, so pinning, unpinning and pinning
        // again gives back the same file.
        const int colourLine = key == u"pinned" ? findKey(inner, spellings(QStringLiteral("colour"))) : -1;
        inner.insert(colourLine >= 0 ? colourLine : inner.size(), key + QStringLiteral(": ") + value);
    }
    bool empty = true;
    for (const QString &line : std::as_const(inner)) {
        if (!isBlank(line)) {
            empty = false;
            break;
        }
    }
    if (empty) {
        return {};
    }
    return open + u'\n' + inner.join(u'\n') + u'\n' + close + (trailingBreak ? QStringLiteral("\n") : QString());
}

void setPinned(NoteParts &parts, bool pinned)
{
    parts.frontMatter = setFrontMatterValue(parts.frontMatter, QStringLiteral("pinned"), pinned ? QStringLiteral("true") : QString());
}

void setColour(NoteParts &parts, const QString &colour)
{
    parts.frontMatter = setFrontMatterValue(parts.frontMatter, QStringLiteral("colour"), colour);
}

QStringList tags(const QString &text)
{
    static const QRegularExpression code(QStringLiteral("`[^`]*`"));
    static const QRegularExpression destination(QStringLiteral(R"(\]\([^)]*\))"));
    static const QRegularExpression tag(QStringLiteral(R"((?:^|(?<=[\s(\[,;]))#([\p{L}\p{N}_][\p{L}\p{N}_/-]*))"),
                                        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression digits(QStringLiteral(R"(^\d+$)"));
    QStringList found;
    bool fenced = false;
    const QStringList lines = text.split(u'\n');
    for (QString line : lines) {
        if (isFence(line)) {
            fenced = !fenced;
            continue;
        }
        if (fenced) {
            continue;
        }
        line.replace(code, QString()).replace(destination, QStringLiteral("]"));
        auto it = tag.globalMatch(line);
        while (it.hasNext()) {
            QString name = it.next().captured(1);
            while (name.endsWith(u'-') || name.endsWith(u'/')) {
                name.chop(1);
            }
            name = name.toLower();
            if (!name.isEmpty() && !digits.match(name).hasMatch() && !found.contains(name)) {
                found.append(name);
            }
        }
    }
    return found;
}

QString preview(const QString &body)
{
    static const QRegularExpression marker(QStringLiteral(R"(^\s*(?:[-*+]\s+\[[ xX]\]\s+|[-*+]\s+|\d+[.)]\s+|>\s*|#{1,6}\s+))"));
    static const QRegularExpression image(QStringLiteral(R"(!\[[^\]]*\]\([^)]*\))"));
    static const QRegularExpression link(QStringLiteral(R"(\[([^\]]*)\]\([^)]*\))"));
    static const QRegularExpression rule(QStringLiteral(R"(^\s*\|?[\s:|-]*-{3,}[\s:|-]*\|?\s*$|^\s*(?:\*\s*){3,}$|^\s*(?:_\s*){3,}$)"));
    static const QRegularExpression tagsOnly(QStringLiteral(R"(<\/?u>)"));
    static const QRegularExpression space(QStringLiteral(R"(\s+)"));
    QString out;
    bool fenced = false;
    const QStringList lines = body.split(u'\n');
    for (QString line : lines) {
        if (isFence(line)) {
            fenced = !fenced;
            continue;
        }
        if (fenced || isBlank(line) || rule.match(line).hasMatch()) {
            continue;
        }
        line.replace(marker, QString());
        line.replace(image, QString());
        line.replace(link, QStringLiteral("\\1"));
        line.replace(tagsOnly, QString());
        line.remove(QStringLiteral("**")).remove(QStringLiteral("__")).remove(QStringLiteral("~~")).remove(u'`').remove(u'*');
        line.replace(u'|', u' ');
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (!out.isEmpty()) {
            out += u' ';
        }
        out += line;
        if (out.size() > 200) {
            break;
        }
    }
    out.replace(space, QStringLiteral(" "));
    return out.left(200);
}

QString safeFileName(const QString &title)
{
    QString name;
    for (const QChar c : title) {
        if (c == u'/' || c == u'\\' || c == u':') {
            name += u'-';
        } else if (c == u'*' || c == u'?' || c == u'"' || c == u'<' || c == u'>' || c == u'|') {
            continue;
        } else if (c.category() == QChar::Other_Control || c == u'\n' || c == u'\t') {
            name += u' ';
        } else {
            name += c;
        }
    }
    name = name.simplified();
    while (name.startsWith(u'.')) {
        name.remove(0, 1);
    }
    while (name.endsWith(u'.') || name.endsWith(u' ')) {
        name.chop(1);
    }
    // Room for ".md" and a " 2" within the 255 bytes a file name may have.
    while (name.toUtf8().size() > 200) {
        name.chop(1);
    }
    name = name.trimmed();
    return name.isEmpty() ? QStringLiteral("Untitled") : name;
}
}
