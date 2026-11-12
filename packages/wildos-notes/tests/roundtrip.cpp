// Checks that a note opened and saved without a change comes back byte for
// byte, through the same path the app takes: the file split into its
// pieces, the text put in a QTextDocument with the Markdown styling on, and
// read back out. Also checks that pinning and unpinning, and typing then
// undoing, leave the file as it was.
//
//   notes-roundtrip [file.md ...]   the built-in samples, then any files

#include "markdowneditor.h"
#include "notefile.h"

#include <QFile>
#include <QGuiApplication>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include <cstdio>

namespace
{
int failures = 0;

void check(bool ok, const QString &name, const QString &what)
{
    std::printf("%s  %s: %s\n", ok ? "ok  " : "FAIL", qPrintable(name), qPrintable(what));
    if (!ok) {
        ++failures;
    }
}

// What the editor holds and hands back, as MarkdownEditor::setText and
// MarkdownEditor::text do it.
QString throughEditor(const QString &body, QTextDocument &doc)
{
    doc.setPlainText(body);
    MarkdownState::applySpacing(&doc, 1, false);
    doc.clearUndoRedoStacks();
    QString raw = doc.toRawText();
    raw.replace(QChar::ParagraphSeparator, u'\n');
    return raw;
}

void roundTrip(const QString &name, const QByteArray &bytes)
{
    NoteParts parts = NoteFile::parse(bytes);
    if (!parts.valid) {
        // Notes shows such a file but never writes it.
        check(true, name, QStringLiteral("not UTF-8, so opened read only"));
        return;
    }
    check(NoteFile::serialize(parts) == bytes, name, QStringLiteral("parse and write back"));

    QTextDocument doc;
    MarkdownHighlighter highlighter(&doc);
    highlighter.ink = Qt::white;
    highlighter.inkMuted = Qt::gray;
    highlighter.inkSubtle = Qt::darkGray;
    highlighter.link = Qt::blue;
    highlighter.fontFamily = QStringLiteral("Albert Sans");
    highlighter.monoFamily = QStringLiteral("monospace");
    highlighter.baseDir = QStringLiteral("/tmp");
    const QString shown = throughEditor(parts.body, doc);
    check(shown == parts.body, name, QStringLiteral("editor shows the text unchanged"));
    NoteParts back = parts;
    back.body = shown;
    check(NoteFile::serialize(back) == bytes, name, QStringLiteral("opened in the editor and saved"));

    // Moving through every line, as the cursor does, restyles lines but
    // never touches the text.
    for (QTextBlock b = doc.begin(); b.isValid(); b = b.next()) {
        highlighter.editingBlock = b.blockNumber();
        highlighter.rehighlightBlock(b);
    }
    QString after = doc.toRawText();
    after.replace(QChar::ParagraphSeparator, u'\n');
    check(after == parts.body, name, QStringLiteral("text unchanged after the cursor visits every line"));

    QTextCursor cursor(&doc);
    cursor.movePosition(QTextCursor::End);
    cursor.beginEditBlock();
    cursor.insertText(QStringLiteral("typed\n\nmore"));
    cursor.endEditBlock();
    MarkdownState::applySpacing(&doc, 1, true);
    doc.undo();
    after = doc.toRawText();
    after.replace(QChar::ParagraphSeparator, u'\n');
    check(after == parts.body, name, QStringLiteral("typing then undo"));

    {
        NoteParts pinned = parts;
        const bool was = NoteFile::pinned(pinned);
        NoteFile::setPinned(pinned, !was);
        check(NoteFile::pinned(pinned) == !was, name, QStringLiteral("pin toggles"));
        NoteFile::setPinned(pinned, was);
        if (!was) {
            check(NoteFile::serialize(pinned) == bytes, name, QStringLiteral("pin then unpin gives the file back"));
        }
        NoteParts titled = parts;
        NoteFile::setTitle(titled, NoteFile::headingText(titled.titleLine));
        if (!titled.titleLine.isEmpty()) {
            check(NoteFile::serialize(titled) == bytes, name, QStringLiteral("same title written again"));
        }
    }
}

struct Sample {
    const char *name;
    QByteArray bytes;
};
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    const QList<Sample> samples{
        {"full note", QByteArray("---\npinned: true\ncolour: amber\n---\n# Open day to-do\n\n"
                                 "Saturday 18 July. Everything below needs a name against it by the AGM.\n\n"
                                 "## Volunteers\n\n"
                                 "- [x] Front desk: two people all day\n  Maya and Tom\n"
                                 "- [ ] Tea tent from 10:00 to 16:00\n  Fiona said yes to the morning\n"
                                 "- [ ] Car park, two marshals\n"
                                 "- [ ] Ring the venue about parking\n  #to-do \xc2\xb7 due Friday\n\n"
                                 "## Kit\n\n"
                                 "* Signs and arrows for the car park\n"
                                 "* Loud hailer and spare batteries\n"
                                 "    * nested four spaces\n"
                                 "\t- nested with a tab\n"
                                 "1. First\n2) Second\n\n"
                                 "```python\nprint('**not bold**')  # not a #tag\n```\n\n"
                                 "| Item | Who |\n| --- | :-: |\n| Urn | Sam |\n\n"
                                 "See [the venue](https://example.org/a_b?c=1#frag) and <https://wildos.io>.\n"
                                 "![map](Images/car%20park.png)\n"
                                 "Some **bold**, *italic*, _also_, ~~gone~~, <u>under</u> and `code`.\n"
                                 "Non\xc2\xa0" "breaking space and emoji \xf0\x9f\x8c\xbb.\n")},
        {"crlf without title", QByteArray("Shopping\r\n\r\n- [ ] eggs\r\n- [x] milk\r\n")},
        {"bom and no final break", QByteArray("\xEF\xBB\xBF# Title only")},
        {"other front matter", QByteArray("---\ntitle: \"Kept\"\ntags: [a, b]\ncolor: Blue\ncreated: 2026-01-02\n---\n\nBody first, no heading.\n")},
        {"empty", QByteArray()},
        {"title and blank lines", QByteArray("\n\n# Title   \n\n\n\ntext\n\n\n")},
        {"closing hashes", QByteArray("# Title ##\ntext straight after\n")},
        {"rule at the top", QByteArray("---\nnot yaml here\n---\nmore\n")},
        {"hard breaks and tabs", QByteArray("Line with two spaces  \nnext\tline\twith tabs\n\n> quote\n> more\n\n***\n")},
        {"setext heading", QByteArray("Title\n=====\n\ntext\n")},
        {"not utf-8", QByteArray("caf\xe9 au lait\n")},
        {"line separator", QByteArray("one\xe2\x80\xa8two\n")},
    };
    for (const Sample &s : samples) {
        roundTrip(QString::fromUtf8(s.name), s.bytes);
    }

    // Pinning a plain note adds front matter; unpinning takes it away.
    {
        const QByteArray plain("# Plain\n\ntext\n");
        NoteParts parts = NoteFile::parse(plain);
        NoteFile::setPinned(parts, true);
        NoteFile::setColour(parts, QStringLiteral("green"));
        check(NoteFile::serialize(parts) == QByteArray("---\npinned: true\ncolour: green\n---\n# Plain\n\ntext\n"), QStringLiteral("front matter"),
              QStringLiteral("added only when set"));
        NoteFile::setPinned(parts, false);
        NoteFile::setColour(parts, QString());
        check(NoteFile::serialize(parts) == plain, QStringLiteral("front matter"), QStringLiteral("gone again when cleared"));
    }
    // A new note: a title, then text under it.
    {
        NoteParts parts = NoteFile::parse(QByteArray());
        NoteFile::setTitle(parts, QStringLiteral("Shopping"));
        check(NoteFile::serialize(parts) == QByteArray("# Shopping\n"), QStringLiteral("new note"), QStringLiteral("title alone"));
        parts.body = QStringLiteral("- [ ] eggs\n");
        check(NoteFile::serialize(parts) == QByteArray("# Shopping\n\n- [ ] eggs\n"), QStringLiteral("new note"), QStringLiteral("title and a checklist"));
    }
    // Tags and the title from the file name.
    {
        const NoteParts parts = NoteFile::parse(samples[0].bytes);
        const QStringList tags = NoteFile::tags(parts.titleLine + u'\n' + parts.body);
        check(tags == QStringList{QStringLiteral("to-do")}, QStringLiteral("tags"), QStringLiteral("found #to-do only: ") + tags.join(u','));
        check(NoteFile::title(NoteFile::parse("text\n"), QStringLiteral("File name")) == u"File name", QStringLiteral("title"),
              QStringLiteral("file name when there is no heading"));
        check(NoteFile::safeFileName(QStringLiteral(" ../a/b: c?* ")) == u"-a-b- c", QStringLiteral("file name"), QStringLiteral("made safe"));
    }

    for (int i = 1; i < argc; ++i) {
        QFile file(QString::fromLocal8Bit(argv[i]));
        if (file.open(QIODevice::ReadOnly)) {
            roundTrip(file.fileName(), file.readAll());
        }
    }
    std::printf("%s\n", failures == 0 ? "All passed" : "Some checks failed");
    return failures == 0 ? 0 : 1;
}
