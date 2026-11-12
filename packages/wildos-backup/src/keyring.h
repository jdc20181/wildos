#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <functional>

// The repository password in the person's keyring (KWallet or the Secret
// Service, through QtKeychain), under "Wild OS Backup". It is the only
// copy on this PC: Backup never writes it anywhere else.
namespace Keyring
{
// ok, password, and why not when !ok. `missing` is true when the keyring
// works but holds no password yet.
using ReadDone = std::function<void(bool ok, const QByteArray &password, bool missing, const QString &why)>;
using WriteDone = std::function<void(bool ok, const QString &why)>;

void read(QObject *context, ReadDone done);
void write(QObject *context, const QByteArray &password, WriteDone done);
// For the scheduled run, which has nothing else to do meanwhile.
bool readBlocking(QByteArray *password, QString *why);

// A new password to write down: 24 small letters and digits in groups of
// four, leaving out the easily misread 0, o, 1 and l. 120 bits.
QByteArray generate();
}
