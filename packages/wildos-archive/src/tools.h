#pragma once

#include "progress.h"
#include "reader.h"

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

// The free archivers Archive hands over to where libarchive cannot
// decrypt: 7-Zip (7z, from the 7zip package) for protected 7z, and The
// Unarchiver (unar and lsar, from the unar package) for protected RAR.
//
// A password never goes on a command line, where every user on the PC can
// read it in /proc. 7z reads it from its stdin when it is not a terminal.
// unar only takes it with -p or at its own prompt, which it shows on a
// terminal, so it runs on a pseudo terminal of its own and is answered
// there.
namespace Tools
{
struct Job {
    QString program;
    QStringList arguments;
    QByteArray password;
    // On a pseudo terminal, answering the tool's password prompt, rather
    // than on pipes with the password on stdin.
    bool terminal = false;
    // Takes stdout as it comes instead of keeping it. Returning false
    // stops the tool.
    std::function<bool(const char *data, qsizetype size)> data;
    // Each line of text the tool prints, for progress.
    std::function<void(const QByteArray &line)> line;
};

struct Outcome {
    bool started = false;
    bool cancelled = false;
    // Stopped because `data` asked.
    bool stopped = false;
    int exitCode = -1;
    // stdout, or everything the terminal showed.
    QByteArray output;
    QByteArray errors;
    bool askedForPassword = false;
    // Asked a second time: the first answer was not taken.
    bool askedAgain = false;
};

bool available(const QString &program);
Outcome run(const Job &job, Progress *progress);

// Overwrites the bytes in place, for passwords: every copy of an
// implicitly shared QByteArray sees the zeros.
void wipe(QByteArray &bytes);
// Removes a folder and everything in it without following any link.
void removeTree(const QString &path);
// A new private folder (0700) inside `parent`, or empty.
QString makeStaging(const QString &parent);

struct Item {
    QString path;
    qint64 size = 0;
    qint64 packed = -1;
    QDateTime modified;
    bool dir = false;
    bool symlink = false;
    bool encrypted = false;
    bool executable = false;
    QString linkTarget;
};

struct Contents {
    QList<Item> items;
    // 7z's "Type" or lsar's format name.
    QString type;
    bool ok = false;
    bool cancelled = false;
    // Even the names are behind a password.
    bool needsPassword = false;
    Reader::PasswordState password = Reader::PasswordState::Fine;
    QString error;
};

// `7z l -slt`, with the password on stdin when there is one.
Contents list7z(const QString &archive, const QByteArray &password, Progress *progress);
// `lsar -j`; lsar cannot take a password safely, so an archive with hidden
// names comes back with needsPassword and is listed with 7z instead.
Contents listLsar(const QString &archive, Progress *progress);

// What a tool said about the password, from its output.
Reader::PasswordState passwordOutcome(const Outcome &outcome, const QByteArray &password);
}
