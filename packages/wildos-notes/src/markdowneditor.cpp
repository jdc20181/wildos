#include "markdowneditor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QImageReader>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>

namespace
{
// Design sizes, in pixels before the system text size is applied.
constexpr qreal kBody = 14.5;
constexpr qreal kSmall = 12.5;
constexpr qreal kCode = 13;
constexpr int kDepthStep = 24;
constexpr int kCheckReserve = 28;
constexpr int kBulletReserve = 22;

constexpr int kNormal = 0;
constexpr int kFence = 1;
constexpr int kListBase = 100;

int listState(int depth, MarkdownLine::Kind kind)
{
    const int code = kind == MarkdownLine::Check ? 1 : kind == MarkdownLine::Ordered ? 2 : 0;
    return kListBase + depth * 10 + code;
}

MarkdownLine::Kind listStateKind(int state)
{
    const int code = (state - kListBase) % 10;
    return code == 1 ? MarkdownLine::Check : code == 2 ? MarkdownLine::Ordered : MarkdownLine::Bullet;
}

int listStateDepth(int state)
{
    return (state - kListBase) / 10;
}

const QRegularExpression &re(const char *pattern)
{
    // Each pattern is compiled once; the cache is keyed by its address.
    static QHash<const char *, QRegularExpression> compiled;
    auto it = compiled.find(pattern);
    if (it == compiled.end()) {
        it = compiled.insert(pattern, QRegularExpression(QString::fromUtf8(pattern), QRegularExpression::UseUnicodePropertiesOption));
    }
    return *it;
}

constexpr const char *kFencePattern = R"(^ {0,3}(```|~~~))";
constexpr const char *kCheckPattern = R"(^([ \t]*)[-*+][ \t]+\[([ xX])\](?:[ \t]+|$))";
constexpr const char *kRulePattern = R"(^ {0,3}([-*_])(?:[ \t]*\1){2,}[ \t]*$)";
constexpr const char *kBulletPattern = R"(^([ \t]*)([-*+])[ \t]+)";
constexpr const char *kOrderedPattern = R"(^([ \t]*)(\d{1,9})([.)])[ \t]+)";
constexpr const char *kHeadingPattern = R"(^(#{1,6})[ \t]+)";
constexpr const char *kImagePattern = R"(^[ \t]*!\[[^\]]*\]\(<?([^)>]+?)>?(?:[ \t]+"[^"]*")?\)[ \t]*$)";
constexpr const char *kTablePattern = R"(^[ \t]*\|)";
constexpr const char *kQuotePattern = R"(^[ \t]*>[ \t]?)";

int columns(QStringView ws)
{
    int cols = 0;
    for (const QChar c : ws) {
        cols += c == u'\t' ? 4 : 1;
    }
    return cols;
}

int leadingWhite(const QString &text)
{
    int n = 0;
    while (n < text.size() && (text[n] == u' ' || text[n] == u'\t')) {
        ++n;
    }
    return n;
}

// A mark tucked out of sight: a one pixel font, each character pulled back
// by its own width, so a hidden link address leaves no gap.
QTextCharFormat hiddenFormat()
{
    QTextCharFormat f;
    f.setProperty(QTextFormat::FontPixelSize, 1);
    f.setForeground(Qt::transparent);
    f.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    return f;
}
}

namespace MarkdownState
{
int listReserve(int depth, MarkdownLine::Kind kind)
{
    return depth * kDepthStep + (kind == MarkdownLine::Check ? kCheckReserve : kind == MarkdownLine::Bullet ? kBulletReserve : 0);
}

bool applySpacing(QTextDocument *doc, qreal scale, bool join)
{
    // A blank line under a heading closes up to the list or text below
    // (about 14px heading to item); any other blank line leaves about 22px
    // between a paragraph and what follows.
    const qreal underHeading = std::round(4 * scale);
    const qreal between = std::round(12 * scale);
    QTextCursor cursor(doc);
    bool started = false;
    QTextBlock previous;
    for (QTextBlock block = doc->begin(); block.isValid(); previous = block, block = block.next()) {
        const int before = previous.isValid() && previous.userState() >= 0 ? previous.userState() : kNormal;
        const bool blank = before != kFence && leadingWhite(block.text()) == block.text().size() && block.next().isValid();
        int type = QTextBlockFormat::ProportionalHeight;
        qreal height = 150;
        if (blank) {
            type = QTextBlockFormat::FixedHeight;
            height = previous.isValid() && re(kHeadingPattern).match(previous.text()).hasMatch() ? underHeading : between;
        }
        QTextBlockFormat format = block.blockFormat();
        if (format.lineHeightType() == type && qFuzzyCompare(format.lineHeight(), height)) {
            continue;
        }
        if (!started) {
            join ? cursor.joinPreviousEditBlock() : cursor.beginEditBlock();
            started = true;
        }
        format.setLineHeight(height, type);
        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }
    if (started) {
        cursor.endEditBlock();
    }
    return started;
}

MarkdownLine analyse(const QString &text, int previousState, int *nextState)
{
    MarkdownLine line;
    int next = kNormal;
    const bool fence = re(kFencePattern).match(text).hasMatch();
    if (previousState == kFence) {
        line.kind = fence ? MarkdownLine::Fence : MarkdownLine::Code;
        *nextState = fence ? kNormal : kFence;
        return line;
    }
    if (fence) {
        line.kind = MarkdownLine::Fence;
        *nextState = kFence;
        return line;
    }
    line.indent = leadingWhite(text);
    if (line.indent == text.size()) {
        line.kind = MarkdownLine::Blank;
        *nextState = kNormal;
        return line;
    }
    line.depth = columns(QStringView(text).left(line.indent)) / 2;

    QRegularExpressionMatch m;
    if ((m = re(kCheckPattern).match(text)).hasMatch()) {
        line.kind = MarkdownLine::Check;
        line.checked = m.captured(2) != u" ";
        line.contentStart = m.capturedEnd(0);
        next = listState(line.depth, line.kind);
    } else if (re(kRulePattern).match(text).hasMatch()) {
        line.kind = MarkdownLine::Rule;
    } else if ((m = re(kBulletPattern).match(text)).hasMatch()) {
        line.kind = MarkdownLine::Bullet;
        line.contentStart = m.capturedEnd(0);
        next = listState(line.depth, line.kind);
    } else if ((m = re(kOrderedPattern).match(text)).hasMatch()) {
        line.kind = MarkdownLine::Ordered;
        line.level = m.captured(2).toInt();
        line.contentStart = m.capturedEnd(0);
        next = listState(line.depth, line.kind);
    } else if ((m = re(kHeadingPattern).match(text)).hasMatch()) {
        line.kind = MarkdownLine::Heading;
        line.level = m.capturedLength(1);
        line.contentStart = m.capturedEnd(0);
    } else if ((m = re(kImagePattern).match(text)).hasMatch()) {
        line.kind = MarkdownLine::Image;
        line.image = m.captured(1).trimmed();
    } else if (re(kTablePattern).match(text).hasMatch()) {
        line.kind = MarkdownLine::Table;
    } else if (previousState >= kListBase && line.indent > 0) {
        line.kind = MarkdownLine::Continuation;
        line.contentStart = line.indent;
        line.reserve = listReserve(listStateDepth(previousState), listStateKind(previousState));
        next = previousState;
    } else if ((m = re(kQuotePattern).match(text)).hasMatch()) {
        line.kind = MarkdownLine::Quote;
        line.contentStart = m.capturedEnd(0);
    } else {
        line.kind = MarkdownLine::Paragraph;
    }
    *nextState = next;
    return line;
}
}

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
}

MarkdownHighlighter::ImageInfo MarkdownHighlighter::imageFor(const QString &destination) const
{
    if (destination.contains(QStringLiteral("://")) || baseDir.isEmpty()) {
        return {};
    }
    const QString path = QDir(baseDir).absoluteFilePath(QUrl::fromPercentEncoding(destination.toUtf8()));
    const QFileInfo info(path);
    if (!info.isFile()) {
        return {};
    }
    const QString key = path + u'@' + QString::number(info.lastModified().toMSecsSinceEpoch()) + u'@' + QString::number(maxImageWidth);
    auto it = m_images.constFind(key);
    if (it != m_images.constEnd()) {
        return *it;
    }
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QSize size = reader.size();
    ImageInfo image;
    if (size.isValid() && !size.isEmpty()) {
        size = size.scaled(QSize(std::min(size.width(), maxImageWidth), std::min(size.height(), 360)), Qt::KeepAspectRatio);
        image = {path, size.width(), size.height()};
    }
    m_images.insert(key, image);
    return image;
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    const int previous = previousBlockState() < 0 ? kNormal : previousBlockState();
    int next = kNormal;
    const MarkdownLine line = MarkdownState::analyse(text, previous, &next);
    setCurrentBlockState(next);
    if (text.isEmpty()) {
        return;
    }
    const bool editing = currentBlock().blockNumber() == editingBlock;
    const qsizetype n = text.size();

    QTextCharFormat base;
    base.setForeground(ink);
    QList<QTextCharFormat> f(n, base);
    QList<bool> fixed(n, false);

    auto merge = [&](qsizetype from, qsizetype to, const QTextCharFormat &with) {
        for (qsizetype i = std::max<qsizetype>(0, from); i < std::min(to, n); ++i) {
            if (!fixed[i]) {
                f[i].merge(with);
            }
        }
    };
    QFont tinyFont(fontFamily);
    tinyFont.setPixelSize(1);
    const QFontMetricsF tiny(tinyFont);
    auto hide = [&](qsizetype from, qsizetype to, int reserve) {
        if (to <= from) {
            return;
        }
        for (qsizetype i = from; i < to; ++i) {
            f[i] = hiddenFormat();
            f[i].setFontLetterSpacing(-tiny.horizontalAdvance(text[i]));
            fixed[i] = true;
        }
        if (reserve > 0) {
            f[to - 1].setFontLetterSpacing(px(reserve) - tiny.horizontalAdvance(text[to - 1]));
        }
    };
    auto colour = [](const QColor &c) {
        QTextCharFormat x;
        x.setForeground(c);
        return x;
    };
    auto sized = [this](qreal size) {
        QTextCharFormat x;
        x.setProperty(QTextFormat::FontPixelSize, int(std::lround(px(size))));
        return x;
    };
    QTextCharFormat mono = sized(kCode);
    mono.setFontFamilies({monoFamily});
    mono.setFontFixedPitch(true);

    bool inline_ = true;
    switch (line.kind) {
    case MarkdownLine::Fence:
        merge(0, n, mono);
        merge(0, n, colour(inkSubtle));
        inline_ = false;
        break;
    case MarkdownLine::Code: {
        QTextCharFormat code = mono;
        code.setBackground(codeFill);
        merge(0, n, code);
        inline_ = false;
        break;
    }
    case MarkdownLine::Heading: {
        if (editing) {
            merge(0, line.contentStart, colour(inkSubtle));
        } else {
            hide(0, line.contentStart, 0);
        }
        const qreal size = line.level == 1 ? 22 : line.level == 2 ? 17 : line.level == 3 ? 15.5 : kBody;
        QTextCharFormat heading = sized(size);
        heading.setFontWeight(line.level <= 2 ? 600 : 550);
        merge(0, n, heading);
        break;
    }
    case MarkdownLine::Check:
    case MarkdownLine::Bullet:
        if (line.depth > 0) {
            hide(0, line.indent, line.depth * kDepthStep);
        }
        hide(line.indent, line.contentStart, line.kind == MarkdownLine::Check ? kCheckReserve : kBulletReserve);
        if (line.checked) {
            QTextCharFormat done = colour(inkMuted);
            done.setFontStrikeOut(true);
            merge(line.contentStart, n, done);
        }
        break;
    case MarkdownLine::Ordered:
        merge(0, line.contentStart, colour(inkMuted));
        break;
    case MarkdownLine::Continuation: {
        hide(0, line.contentStart, line.reserve);
        QTextCharFormat note = sized(kSmall);
        note.setForeground(inkMuted);
        merge(line.contentStart, n, note);
        break;
    }
    case MarkdownLine::Table: {
        merge(0, n, mono);
        const bool divider = re(R"(^[\s|:-]+$)").match(text).hasMatch();
        for (qsizetype i = 0; i < n; ++i) {
            if (text[i] == u'|' || (divider && text[i] == u'-')) {
                f[i].setForeground(inkSubtle);
            }
        }
        inline_ = false;
        break;
    }
    case MarkdownLine::Image: {
        const ImageInfo image = imageFor(line.image);
        if (image.height <= 0) {
            merge(0, n, colour(inkMuted));
            inline_ = false;
            break;
        }
        // The line is made as tall as the picture with one large, clear
        // character, and the editor draws the picture over it. The link
        // shows only while the cursor is on the line.
        // Every line is set at one and a half times its height, so the
        // character is a third smaller than the picture is tall.
        QFont probe(fontFamily);
        probe.setPixelSize(100);
        const qreal ratio = QFontMetricsF(probe).height() / 100.0 * 1.5;
        if (editing) {
            QTextCharFormat link = sized(kSmall);
            link.setForeground(inkSubtle);
            merge(0, n, link);
        } else {
            hide(0, n, 0);
        }
        QTextCharFormat tall;
        tall.setProperty(QTextFormat::FontPixelSize, int(std::ceil((image.height + 12) / ratio)));
        tall.setForeground(Qt::transparent);
        tall.setFontLetterSpacingType(QFont::AbsoluteSpacing);
        tall.setFontLetterSpacing(0);
        f[line.indent] = tall;
        fixed[line.indent] = true;
        inline_ = false;
        break;
    }
    case MarkdownLine::Quote: {
        merge(0, line.contentStart, colour(inkSubtle));
        QTextCharFormat quote = colour(inkMuted);
        quote.setFontItalic(true);
        merge(line.contentStart, n, quote);
        break;
    }
    case MarkdownLine::Rule:
        merge(0, n, colour(inkSubtle));
        inline_ = false;
        break;
    case MarkdownLine::Paragraph:
        merge(0, n, colour(inkMuted));
        break;
    case MarkdownLine::Blank:
        inline_ = false;
        break;
    }

    if (inline_) {
        const qsizetype from = line.contentStart;
        QList<bool> code(n, false);
        auto marks = [&](qsizetype a, qsizetype b) {
            if (editing) {
                merge(a, b, colour(inkSubtle));
            } else {
                hide(a, b, 0);
            }
        };
        auto spans = [&](const char *pattern, int markLength, const QTextCharFormat &style, bool isCode = false) {
            auto it = re(pattern).globalMatch(text, from);
            while (it.hasNext()) {
                const auto m = it.next();
                const qsizetype a = m.capturedStart(0);
                const qsizetype b = m.capturedEnd(0);
                if (!isCode && (code[a] || code[b - 1])) {
                    continue;
                }
                merge(a + markLength, b - markLength, style);
                marks(a, a + markLength);
                marks(b - markLength, b);
                if (isCode) {
                    for (qsizetype i = a; i < b; ++i) {
                        code[i] = true;
                    }
                }
            }
        };
        QTextCharFormat codeStyle = mono;
        codeStyle.setBackground(codeFill);
        spans(R"(`[^`]+`)", 1, codeStyle, true);
        QTextCharFormat bold;
        bold.setFontWeight(600);
        spans(R"(\*\*(?=\S)(?:.+?)(?<=\S)\*\*)", 2, bold);
        spans(R"((?<![\w_])__(?=\S)(?:.+?)(?<=\S)__(?![\w_]))", 2, bold);
        QTextCharFormat italic;
        italic.setFontItalic(true);
        spans(R"((?<![*\w])\*(?=[^\s*])(?:[^*]+?)(?<=[^\s*])\*(?![*\w]))", 1, italic);
        spans(R"((?<![_\w])_(?=[^\s_])(?:[^_]+?)(?<=[^\s_])_(?![_\w]))", 1, italic);
        QTextCharFormat strike;
        strike.setFontStrikeOut(true);
        spans(R"(~~(?=\S)(?:.+?)(?<=\S)~~)", 2, strike);
        QTextCharFormat underline;
        underline.setFontUnderline(true);
        auto it = re(R"(<u>(.+?)</u>)").globalMatch(text, from);
        while (it.hasNext()) {
            const auto m = it.next();
            merge(m.capturedStart(1), m.capturedEnd(1), underline);
            marks(m.capturedStart(0), m.capturedStart(1));
            marks(m.capturedEnd(1), m.capturedEnd(0));
        }

        // Links: the words in the link colour, the brackets and address
        // tucked away unless the line is being edited.
        it = re(R"((!?)\[([^\]]*)\]\(([^)]*)\))").globalMatch(text, from);
        while (it.hasNext()) {
            const auto m = it.next();
            if (code[m.capturedStart(0)]) {
                continue;
            }
            if (!m.captured(1).isEmpty()) {
                merge(m.capturedStart(0), m.capturedEnd(0), colour(inkSubtle));
                continue;
            }
            merge(m.capturedStart(2), m.capturedEnd(2), colour(link));
            marks(m.capturedStart(0), m.capturedStart(2));
            marks(m.capturedEnd(2), m.capturedEnd(0));
        }
        it = re(R"((?<![(<\w])https?://[^\s<>()]+)").globalMatch(text, from);
        while (it.hasNext()) {
            const auto m = it.next();
            if (!code[m.capturedStart(0)]) {
                merge(m.capturedStart(0), m.capturedEnd(0), colour(link));
            }
        }
        QTextCharFormat tag;
        tag.setForeground(inkMuted);
        it = re(R"((?:^|(?<=[\s(\[,;]))#[\p{L}\p{N}_][\p{L}\p{N}_/-]*)").globalMatch(text, from);
        while (it.hasNext()) {
            const auto m = it.next();
            if (!code[m.capturedStart(0)]) {
                merge(m.capturedStart(0), m.capturedEnd(0), tag);
            }
        }
    }

    qsizetype start = 0;
    for (qsizetype i = 1; i <= n; ++i) {
        if (i == n || !(f[i] == f[start])) {
            setFormat(int(start), int(i - start), f[start]);
            start = i;
        }
    }
}

MarkdownEditor::MarkdownEditor(QObject *parent)
    : QObject(parent)
{
    m_markerTimer.setSingleShot(true);
    m_markerTimer.setInterval(0);
    connect(&m_markerTimer, &QTimer::timeout, this, &MarkdownEditor::rebuildMarkers);
    m_spacingTimer.setSingleShot(true);
    m_spacingTimer.setInterval(0);
    connect(&m_spacingTimer, &QTimer::timeout, this, [this]() {
        if (QTextDocument *doc = textDocument()) {
            m_spacing = true;
            MarkdownState::applySpacing(doc, m_scale, true);
            m_spacing = false;
        }
    });
    m_styleTimer.setSingleShot(true);
    m_styleTimer.setInterval(0);
    connect(&m_styleTimer, &QTimer::timeout, this, &MarkdownEditor::applyStyle);
}

QQuickTextDocument *MarkdownEditor::document() const
{
    return m_document;
}

void MarkdownEditor::setDocument(QQuickTextDocument *document)
{
    if (document == m_document) {
        return;
    }
    if (m_highlighter) {
        m_highlighter->setDocument(nullptr);
        m_highlighter->deleteLater();
        m_highlighter = nullptr;
    }
    m_document = document;
    if (QTextDocument *doc = textDocument()) {
        m_highlighter = new MarkdownHighlighter(doc);
        connect(doc, &QTextDocument::contentsChanged, &m_markerTimer, qOverload<>(&QTimer::start));
        // Restyling reports a change too; the pass then finds every line
        // already right and changes nothing.
        connect(doc, &QTextDocument::contentsChange, this, [this]() {
            if (!m_spacing) {
                m_spacingTimer.start();
            }
        });
        applyStyle();
    }
    Q_EMIT documentChanged();
}

QTextDocument *MarkdownEditor::textDocument() const
{
    return m_document ? m_document->textDocument() : nullptr;
}

int MarkdownEditor::cursorPosition() const
{
    return m_cursorPosition;
}

void MarkdownEditor::setCursorPosition(int position)
{
    if (position == m_cursorPosition) {
        return;
    }
    m_cursorPosition = position;
    QTextDocument *doc = textDocument();
    if (doc && m_highlighter) {
        const QTextBlock block = doc->findBlock(position);
        const int number = block.isValid() ? block.blockNumber() : -1;
        if (number != m_highlighter->editingBlock) {
            const QTextBlock old = doc->findBlockByNumber(m_highlighter->editingBlock);
            m_highlighter->editingBlock = number;
            if (old.isValid()) {
                m_highlighter->rehighlightBlock(old);
            }
            if (block.isValid()) {
                m_highlighter->rehighlightBlock(block);
            }
        }
    }
    Q_EMIT cursorPositionChanged();
}

QString MarkdownEditor::baseDir() const
{
    return m_baseDir;
}

void MarkdownEditor::setBaseDir(const QString &dir)
{
    if (dir == m_baseDir) {
        return;
    }
    m_baseDir = dir;
    Q_EMIT styleChanged();
    m_styleTimer.start();
}

void MarkdownEditor::setInk(const QColor &value)
{
    if (m_ink == value) {
        return;
    }
    m_ink = value;
    Q_EMIT styleChanged();
    m_styleTimer.start();
}

void MarkdownEditor::setInkMuted(const QColor &value)
{
    if (m_inkMuted == value) {
        return;
    }
    m_inkMuted = value;
    Q_EMIT styleChanged();
    m_styleTimer.start();
}

void MarkdownEditor::setInkSubtle(const QColor &value)
{
    if (m_inkSubtle == value) {
        return;
    }
    m_inkSubtle = value;
    Q_EMIT styleChanged();
    m_styleTimer.start();
}

void MarkdownEditor::setLink(const QColor &value)
{
    if (m_link == value) {
        return;
    }
    m_link = value;
    Q_EMIT styleChanged();
    m_styleTimer.start();
}

void MarkdownEditor::setCodeFill(const QColor &value)
{
    if (m_codeFill == value) {
        return;
    }
    m_codeFill = value;
    Q_EMIT styleChanged();
    m_styleTimer.start();
}

void MarkdownEditor::setFontFamily(const QString &value)
{
    if (m_fontFamily == value) {
        return;
    }
    m_fontFamily = value;
    Q_EMIT styleChanged();
    m_styleTimer.start();
}

void MarkdownEditor::setScale(qreal value)
{
    if (m_scale == value) {
        return;
    }
    m_scale = value;
    Q_EMIT styleChanged();
    m_styleTimer.start();
}

void MarkdownEditor::setMaxImageWidth(int value)
{
    if (m_maxImageWidth == value) {
        return;
    }
    m_maxImageWidth = value;
    Q_EMIT styleChanged();
    m_styleTimer.start();
}

void MarkdownEditor::applyStyle()
{
    if (!m_highlighter) {
        return;
    }
    m_highlighter->ink = m_ink;
    m_highlighter->inkMuted = m_inkMuted;
    m_highlighter->inkSubtle = m_inkSubtle;
    m_highlighter->link = m_link;
    m_highlighter->codeFill = m_codeFill;
    m_highlighter->fontFamily = m_fontFamily;
    m_highlighter->monoFamily = QStringLiteral("monospace");
    m_highlighter->scale = m_scale;
    m_highlighter->baseDir = m_baseDir;
    m_highlighter->maxImageWidth = m_maxImageWidth;
    m_highlighter->rehighlight();
    rebuildMarkers();
}

QVariantList MarkdownEditor::markers() const
{
    return m_markers;
}

MarkdownLine MarkdownEditor::lineAt(const QTextBlock &block) const
{
    const QTextBlock previous = block.previous();
    const int state = previous.isValid() && previous.userState() >= 0 ? previous.userState() : kNormal;
    int next = 0;
    return MarkdownState::analyse(block.text(), state, &next);
}

void MarkdownEditor::rebuildMarkers()
{
    QVariantList markers;
    QTextDocument *doc = textDocument();
    if (doc && m_highlighter) {
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
            const MarkdownLine line = lineAt(block);
            const int at = block.position() + line.indent;
            if (line.kind == MarkdownLine::Check) {
                markers.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("check")}, {QStringLiteral("pos"), at}, {QStringLiteral("checked"), line.checked}});
            } else if (line.kind == MarkdownLine::Bullet) {
                markers.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("bullet")}, {QStringLiteral("pos"), at}});
            } else if (line.kind == MarkdownLine::Image) {
                const auto image = m_highlighter->imageFor(line.image);
                if (image.height > 0) {
                    markers.append(QVariantMap{
                        {QStringLiteral("kind"), QStringLiteral("image")},
                        {QStringLiteral("pos"), at},
                        {QStringLiteral("block"), block.blockNumber()},
                        {QStringLiteral("source"), QUrl::fromLocalFile(image.path).toString()},
                        {QStringLiteral("width"), image.width},
                        {QStringLiteral("height"), image.height},
                    });
                }
            }
        }
    }
    if (markers != m_markers) {
        m_markers = markers;
        Q_EMIT markersChanged();
    }
}

QString MarkdownEditor::text() const
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return {};
    }
    QString raw = doc->toRawText();
    raw.replace(QChar::ParagraphSeparator, u'\n');
    return raw;
}

void MarkdownEditor::setText(const QString &text)
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return;
    }
    if (m_highlighter) {
        m_highlighter->editingBlock = -1;
    }
    m_spacing = true;
    doc->setPlainText(text);
    MarkdownState::applySpacing(doc, m_scale, false);
    m_spacing = false;
    m_spacingTimer.stop();
    doc->clearUndoRedoStacks();
    rebuildMarkers();
}

int MarkdownEditor::cursorBlock() const
{
    return m_highlighter ? m_highlighter->editingBlock : -1;
}

int MarkdownEditor::clampCursor(int position, int previous) const
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return position;
    }
    const QTextBlock block = doc->findBlock(position);
    if (!block.isValid()) {
        return position;
    }
    const MarkdownLine line = lineAt(block);
    const bool hidesStart = line.kind == MarkdownLine::Check || line.kind == MarkdownLine::Bullet || line.kind == MarkdownLine::Continuation;
    const int contentStart = block.position() + line.contentStart;
    if (hidesStart && position < contentStart) {
        if (previous >= contentStart && position < previous && block.position() > 0) {
            return block.position() - 1;
        }
        return contentStart;
    }
    return position;
}

int MarkdownEditor::handleReturn(int position, bool shift)
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return -1;
    }
    const QTextBlock block = doc->findBlock(position);
    MarkdownLine line = lineAt(block);
    const QString text = block.text();
    const QString indent = text.left(line.indent);
    QTextCursor cursor(doc);
    cursor.setPosition(position);

    if (line.kind == MarkdownLine::Continuation) {
        // Enter under an item starts the next item of the same list.
        const QTextBlock previous = block.previous();
        const int state = previous.isValid() ? previous.userState() : kNormal;
        if (state < kListBase) {
            return -1;
        }
        line.kind = listStateKind(state);
        const QString itemIndent = QString(listStateDepth(state) * 2, u' ');
        const QString prefix = shift ? indent : itemIndent + (line.kind == MarkdownLine::Check ? QStringLiteral("- [ ] ") : QStringLiteral("- "));
        cursor.insertText(u'\n' + prefix);
        return cursor.position();
    }
    if (line.kind != MarkdownLine::Bullet && line.kind != MarkdownLine::Check && line.kind != MarkdownLine::Ordered) {
        return -1;
    }
    if (shift) {
        // A line under the item, lined up with its text.
        cursor.insertText(u'\n' + indent + QStringLiteral("  "));
        return cursor.position();
    }
    const QString content = text.mid(line.contentStart);
    if (content.trimmed().isEmpty() && position >= block.position() + line.contentStart) {
        // Enter on an empty item ends the list, with a blank line after it
        // so what comes next is not read as part of the last item.
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + text.size(), QTextCursor::KeepAnchor);
        cursor.insertText(line.depth > 0 ? QString() : QString(u'\n'));
        return cursor.position();
    }
    QString marker;
    if (line.kind == MarkdownLine::Check) {
        marker = QStringLiteral("- [ ] ");
    } else if (line.kind == MarkdownLine::Bullet) {
        marker = text.mid(line.indent, 1) + u' ';
    } else {
        const auto m = re(kOrderedPattern).match(text);
        marker = QString::number(m.captured(2).toInt() + 1) + m.captured(3) + u' ';
    }
    cursor.insertText(u'\n' + indent + marker);
    return cursor.position();
}

int MarkdownEditor::handleBackspace(int position)
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return -1;
    }
    const QTextBlock block = doc->findBlock(position);
    const MarkdownLine line = lineAt(block);
    const int contentStart = block.position() + line.contentStart;
    if (position != contentStart) {
        return -1;
    }
    QTextCursor cursor(doc);
    if (line.kind == MarkdownLine::Check || line.kind == MarkdownLine::Bullet || line.kind == MarkdownLine::Ordered) {
        // Backspace at the start of an item's text takes the box or bullet
        // away and leaves the words.
        cursor.setPosition(block.position() + line.indent);
        cursor.setPosition(contentStart, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        return cursor.position();
    }
    if (line.kind == MarkdownLine::Continuation && block.previous().isValid()) {
        cursor.setPosition(block.previous().position() + block.previous().length() - 1);
        cursor.setPosition(contentStart, QTextCursor::KeepAnchor);
        cursor.insertText(QStringLiteral(" "));
        return cursor.position() - 1;
    }
    return -1;
}

int MarkdownEditor::handleTab(int position, bool back)
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return -1;
    }
    const QTextBlock block = doc->findBlock(position);
    const MarkdownLine line = lineAt(block);
    if (line.kind != MarkdownLine::Check && line.kind != MarkdownLine::Bullet && line.kind != MarkdownLine::Ordered) {
        return -1;
    }
    QTextCursor cursor(doc);
    cursor.setPosition(block.position());
    if (back) {
        const int remove = std::min(2, line.indent);
        if (remove == 0) {
            return position;
        }
        cursor.setPosition(block.position() + remove, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        return std::max(block.position(), position - remove);
    }
    cursor.insertText(QStringLiteral("  "));
    return position + 2;
}

void MarkdownEditor::toggleCheck(int markerPosition)
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return;
    }
    const QTextBlock block = doc->findBlock(markerPosition);
    const MarkdownLine line = lineAt(block);
    if (line.kind != MarkdownLine::Check) {
        return;
    }
    const int box = block.text().indexOf(u'[', line.indent);
    if (box < 0) {
        return;
    }
    QTextCursor cursor(doc);
    cursor.setPosition(block.position() + box + 1);
    cursor.setPosition(block.position() + box + 2, QTextCursor::KeepAnchor);
    cursor.insertText(line.checked ? QStringLiteral(" ") : QStringLiteral("x"));
}

int MarkdownEditor::toggleHeading(int position)
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return position;
    }
    const QTextBlock block = doc->findBlock(position);
    const MarkdownLine line = lineAt(block);
    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    const int offset = position - block.position();
    int shift = 0;
    // The title is the note's only first-level heading, so the button goes
    // from a heading to a subheading to plain text.
    if (line.kind == MarkdownLine::Heading) {
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + line.contentStart, QTextCursor::KeepAnchor);
        if (line.level < 3) {
            cursor.insertText(QStringLiteral("### "));
            shift = 4 - line.contentStart;
        } else {
            cursor.removeSelectedText();
            shift = -line.contentStart;
        }
    } else {
        int strip = 0;
        if (line.kind == MarkdownLine::Check || line.kind == MarkdownLine::Bullet || line.kind == MarkdownLine::Ordered || line.kind == MarkdownLine::Quote) {
            strip = line.contentStart;
        }
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + strip, QTextCursor::KeepAnchor);
        cursor.insertText(QStringLiteral("## "));
        shift = 3 - strip;
    }
    cursor.endEditBlock();
    return std::max(block.position(), block.position() + offset + shift);
}

QVariantMap MarkdownEditor::toggleList(int start, int end, const QString &kind)
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return {};
    }
    const bool check = kind == u"check";
    QTextBlock first = doc->findBlock(std::min(start, end));
    const QTextBlock last = doc->findBlock(std::max(start, end));
    const bool single = first == last;
    bool all = true;
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        const MarkdownLine line = lineAt(b);
        if (line.kind != MarkdownLine::Blank || single) {
            all = all && line.kind == (check ? MarkdownLine::Check : MarkdownLine::Bullet);
        }
        if (b == last) {
            break;
        }
    }
    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    int delta = 0;
    int firstDelta = 0;
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        const MarkdownLine line = lineAt(b);
        const bool isLast = b == last;
        if (line.kind == MarkdownLine::Blank && !single) {
            if (isLast) {
                break;
            }
            continue;
        }
        int strip = 0;
        if (line.kind == MarkdownLine::Check || line.kind == MarkdownLine::Bullet || line.kind == MarkdownLine::Ordered) {
            strip = line.contentStart - line.indent;
        }
        const QString marker = all ? QString() : check ? QStringLiteral("- [ ] ") : QStringLiteral("- ");
        cursor.setPosition(b.position() + line.indent);
        cursor.setPosition(b.position() + line.indent + strip, QTextCursor::KeepAnchor);
        cursor.insertText(marker);
        const int change = int(marker.size()) - strip;
        if (b == first) {
            firstDelta = change;
        }
        delta += change;
        if (isLast) {
            break;
        }
    }
    cursor.endEditBlock();
    const int from = std::max(first.position(), std::min(start, end) + firstDelta);
    return {{QStringLiteral("start"), from}, {QStringLiteral("end"), std::max(start, end) + delta}};
}

QVariantMap MarkdownEditor::wrap(int start, int end, const QString &open, const QString &close)
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return {};
    }
    const int a = std::min(start, end);
    const int b = std::max(start, end);
    const QString all = text();
    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    QVariantMap result;
    if (a >= open.size() && all.mid(a - open.size(), open.size()) == open && all.mid(b, close.size()) == close) {
        cursor.setPosition(b);
        cursor.setPosition(b + int(close.size()), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.setPosition(a - int(open.size()));
        cursor.setPosition(a, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        result = {{QStringLiteral("start"), a - int(open.size())}, {QStringLiteral("end"), b - int(open.size())}};
    } else {
        cursor.setPosition(b);
        cursor.insertText(close);
        cursor.setPosition(a);
        cursor.insertText(open);
        result = {{QStringLiteral("start"), a + int(open.size())}, {QStringLiteral("end"), b + int(open.size())}};
    }
    cursor.endEditBlock();
    return result;
}

QVariantMap MarkdownEditor::insertLink(int start, int end)
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return {};
    }
    const int a = std::min(start, end);
    const int b = std::max(start, end);
    QString words = text().mid(a, b - a);
    QString address = QStringLiteral("https://");
    if (re(R"(^https?://\S+$)").match(words).hasMatch()) {
        address = words;
    }
    if (words.isEmpty()) {
        words = tr("link");
    }
    QTextCursor cursor(doc);
    cursor.setPosition(a);
    cursor.setPosition(b, QTextCursor::KeepAnchor);
    cursor.insertText(u'[' + words + QStringLiteral("](") + address + u')');
    const int addressStart = a + int(words.size()) + 3;
    return {{QStringLiteral("start"), addressStart}, {QStringLiteral("end"), addressStart + int(address.size())}};
}

int MarkdownEditor::insertTable(int position)
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return position;
    }
    const QTextBlock block = doc->findBlock(position);
    QTextCursor cursor(doc);
    QString table = tr("| Column 1 | Column 2 | Column 3 |") + QStringLiteral("\n| --- | --- | --- |\n|  |  |  |");
    if (block.text().trimmed().isEmpty()) {
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
    } else {
        cursor.setPosition(block.position() + block.length() - 1);
        table.prepend(QStringLiteral("\n\n"));
    }
    const int start = cursor.selectionStart();
    cursor.insertText(table);
    return start + int(table.lastIndexOf(QStringLiteral("|  |"))) + 2;
}

int MarkdownEditor::insertImage(int position, const QUrl &file)
{
    QTextDocument *doc = textDocument();
    const QString source = file.toLocalFile();
    if (!doc || m_baseDir.isEmpty() || !QFileInfo(source).isFile()) {
        return -1;
    }
    const QFileInfo from(source);
    const QDir folder(m_baseDir + QStringLiteral("/Images"));
    folder.mkpath(QStringLiteral("."));
    QString target = folder.filePath(from.fileName());
    for (int i = 2; QFileInfo::exists(target); ++i) {
        if (QFileInfo(target).size() == from.size() && QFileInfo(target).absoluteFilePath() != from.absoluteFilePath()) {
            QFile a(target);
            QFile b(source);
            if (a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly) && a.readAll() == b.readAll()) {
                break;
            }
        }
        target = folder.filePath(from.completeBaseName() + u' ' + QString::number(i) + u'.' + from.suffix());
    }
    if (!QFileInfo::exists(target) && !QFile::copy(source, target)) {
        return -1;
    }
    const QString name = QFileInfo(target).fileName();
    const QString link = QStringLiteral("Images/") + QString::fromUtf8(QUrl::toPercentEncoding(name));
    const QString line = QStringLiteral("![") + from.completeBaseName() + QStringLiteral("](") + link + u')';
    const QTextBlock block = doc->findBlock(position);
    QTextCursor cursor(doc);
    if (block.text().trimmed().isEmpty()) {
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
        cursor.insertText(line + u'\n');
    } else {
        cursor.setPosition(block.position() + block.length() - 1);
        cursor.insertText(u'\n' + line + u'\n');
    }
    return cursor.position();
}

QString MarkdownEditor::linkAt(int position) const
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return {};
    }
    const QTextBlock block = doc->findBlock(position);
    const QString text = block.text();
    const int offset = position - block.position();
    auto it = re(R"(\[([^\]]*)\]\(<?([^)>\s]*)>?[^)]*\)|https?://[^\s<>()]+)").globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        if (offset < m.capturedStart(0) || offset > m.capturedEnd(0)) {
            continue;
        }
        const QString address = m.captured(2).isEmpty() ? m.captured(0) : m.captured(2);
        if (address.contains(QStringLiteral("://")) || address.startsWith(QStringLiteral("mailto:"))) {
            return address;
        }
        if (!m_baseDir.isEmpty()) {
            return QUrl::fromLocalFile(QDir(m_baseDir).absoluteFilePath(QUrl::fromPercentEncoding(address.toUtf8()))).toString();
        }
    }
    return {};
}

QString MarkdownEditor::lineKind(int position) const
{
    QTextDocument *doc = textDocument();
    if (!doc) {
        return {};
    }
    switch (lineAt(doc->findBlock(position)).kind) {
    case MarkdownLine::Heading:
        return QStringLiteral("heading");
    case MarkdownLine::Bullet:
        return QStringLiteral("bullet");
    case MarkdownLine::Check:
        return QStringLiteral("check");
    default:
        return {};
    }
}

void MarkdownEditor::undo()
{
    if (QTextDocument *doc = textDocument()) {
        m_spacing = true;
        doc->undo();
        m_spacing = false;
        m_spacingTimer.stop();
    }
}

void MarkdownEditor::redo()
{
    if (QTextDocument *doc = textDocument()) {
        m_spacing = true;
        doc->redo();
        m_spacing = false;
        m_spacingTimer.stop();
    }
}
