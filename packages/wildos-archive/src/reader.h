#pragma once

#include "progress.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>

// Everything Archive does to an archive file, on libarchive. Each call
// opens the file afresh and runs to the end in the calling thread, so the
// callers run them on worker threads. Nothing here touches the network.

struct Entry {
    // The path inside the archive, cleaned: no leading ./ or /, no
    // trailing /. An unsafe entry keeps the path it was stored with.
    QString path;
    QString name;
    int parent = -1;
    QList<int> children;
    qint64 size = 0;
    // -1 when the format packs files together and has no figure per file.
    qint64 packed = -1;
    QDateTime modified;
    bool dir = false;
    bool symlink = false;
    QString linkTarget;
    bool encrypted = false;
    // An absolute path or one with .. in it: shown, never extracted.
    bool unsafe = false;
    bool image = false;
    // Where the entry comes in the archive, or -1 for a folder that only
    // appears in other entries' paths.
    int order = -1;
    int fileCount = 0;
    // Marked executable in an archive a tool reads; unar does not keep it.
    bool executable = false;
    // The name 7-Zip knows the entry by, to ask it for just this one.
    QString toolPath;
};

struct Listing {
    QList<Entry> entries;
    QList<int> roots;
    // entries index for each order.
    QList<int> byOrder;
    QString format;
    qint64 totalSize = 0;
    qint64 packedSize = 0;
    int fileCount = 0;
    bool raw = false;
    bool encrypted = false;
    bool packedPerFile = false;
    QString error;
    // 7z and RAR can hide even the names behind a password: nothing is
    // listed until it is given.
    bool lockedHeaders = false;
    // Who reads the archive: empty for libarchive, "7z" for 7-Zip, "unar"
    // for The Unarchiver. Entries' orders are then the tool's indexes.
    QString tool;
    // Protected in a way no free tool can decrypt: RAR 4's encryption.
    bool cantDecrypt = false;
};

namespace Reader
{
enum class PasswordState { Fine, Needed, Wrong };

// Sanity of a path from an archive: false for an absolute path, a drive
// letter or a .. step. `clean` gets the path without ./ and empty steps.
bool safePath(const QString &stored, QString *clean);

// libarchive first; 7-Zip or The Unarchiver only for what libarchive
// cannot decrypt or read. The password is for names behind one.
Listing list(const QString &path, const QByteArray &password, Progress *progress);
inline Listing list(const QString &path, Progress *progress)
{
    return list(path, QByteArray(), progress);
}

// Tries the password on a little of the archive. When the names were
// behind it, `relisted` gets the archive listed with it.
struct Unlock {
    PasswordState state = PasswordState::Fine;
    bool relisted = false;
    Listing listing;
};
Unlock unlock(const QString &path, const Listing &listing, const QByteArray &password, Progress *progress);

struct ExtractOptions {
    QString archive;
    QByteArray password;
    QString destination;
    // Paths under this folder of the archive land straight in destination.
    QString stripPrefix;
    bool keepStructure = true;
    bool replace = false;
    // Orders to extract; empty for everything.
    QSet<int> orders;
};

struct Result {
    bool ok = true;
    bool cancelled = false;
    PasswordState password = PasswordState::Fine;
    QString error;
    int written = 0;
    int kept = 0;
    // Entries refused because they would land outside the folder.
    QStringList unsafe;
    // Entries that could not be read or written, with why.
    QStringList failed;
    QString firstWritten;
};

Result extract(const ExtractOptions &options, const Listing &listing, Progress *progress);
// Decompresses every file to check it, writing nothing.
Result test(const QString &path, const QByteArray &password, const Listing &listing, Progress *progress);
// Reads one file into memory, for opening it from the archive.
Result readOne(const QString &path, const QByteArray &password, const Listing &listing, int order, const QString &target, Progress *progress);
// Calls `found` with a small picture for each wanted image, in archive
// order.
void thumbnails(const QString &path, const QByteArray &password, const Listing &listing, const QSet<int> &orders, QSize box,
                Progress *progress, const std::function<void(int order, const QImage &image)> &found);

struct CreateOptions {
    QString target;
    // "zip" or "7z".
    QString format;
    QStringList paths;
    // An archive whose contents go in first, for Add files.
    QString source;
    QByteArray sourcePassword;
};
Result create(const CreateOptions &options, Progress *progress);
}
