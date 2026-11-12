#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// A note on disk is a plain Markdown file. It is split into the pieces Notes
// edits separately and put back together from exactly those pieces, so a
// note that is opened and saved without a change comes back byte for byte.
struct NoteParts {
    QByteArray bom;
    // Every line break was \r\n; the pieces below hold \n only.
    bool crlf = false;
    // "---" to "---" (or "...") at the very top, both fences and the line
    // break after the closing one included. Empty when there is none.
    QString frontMatter;
    // Blank lines above the title heading.
    QString lead;
    // The "# Title" line as written, without its line break. Empty when the
    // note has no title heading and the file name is its title.
    QString titleLine;
    // The line break after the title line and any blank lines after it.
    QString gap;
    QString body;
    // The title line was added by Notes, so it ends with a line break even
    // while the note has no text under it.
    bool addedTitle = false;
    // False when the file isn't UTF-8 text; Notes then shows it but never
    // writes it.
    bool valid = true;
};

namespace NoteFile
{
NoteParts parse(const QByteArray &bytes);
QByteArray serialize(const NoteParts &parts);

QString headingText(const QString &titleLine);
// The title is the first heading, or the file name without .md.
QString title(const NoteParts &parts, const QString &fileBaseName);
void setTitle(NoteParts &parts, const QString &title);

QString frontMatterValue(const QString &frontMatter, const QString &key);
bool pinned(const NoteParts &parts);
QString colour(const NoteParts &parts);
// An empty value takes the key out; the block goes when nothing is left in
// it, so a plain note stays plain.
QString setFrontMatterValue(const QString &frontMatter, const QString &key, const QString &value);
void setPinned(NoteParts &parts, bool pinned);
void setColour(NoteParts &parts, const QString &colour);

// #word tags in the text, lower case, outside code.
QStringList tags(const QString &text);
// The first words of the note for the list, Markdown marks taken out.
QString preview(const QString &body);
// A title made safe as a file name: no slashes, no leading dot, no control
// characters, short enough for any file system.
QString safeFileName(const QString &title);
}
