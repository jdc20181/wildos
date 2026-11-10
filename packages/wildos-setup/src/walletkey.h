#pragma once

#include <QByteArray>
#include <QString>

#include <sys/types.h>

struct passwd;

/* The key that opens an account's KWallet, and the root-held copy of it
 * kept for the one account that signs in automatically.
 *
 * pam_kwallet makes the key from the password typed at sign-in and the
 * salt in ~/.local/share/kwalletd/kdewallet.salt, then has kwalletd open
 * the wallet with it. An automatic sign-in types no password, so for that
 * account Wild OS keeps the same key in /var/lib/wildos/wallet/<uid>.key,
 * readable by root only, on the encrypted root filesystem. Everything here
 * that writes there runs as root.
 */
namespace WalletKey
{
// pam_kwallet's own numbers (KWALLET_PAM_* in kwallet-pam's pam_kwallet.c).
// The key made here has to be the one it makes from the same password, or
// the wallet opens with one and not the other.
constexpr int saltSize = 56;
constexpr int keySize = 56;
constexpr int iterations = 50000;

QByteArray derive(const QByteArray &password, const QByteArray &salt);

/* The account's kdewallet.salt, made first when it has none, the way
 * pam_kwallet makes it: by a child running as the account, so a link in the
 * home folder cannot point root at another file. Empty on failure. */
QByteArray accountSalt(const struct passwd *pw);

/* The account SDDM signs in automatically, as SDDM reads its files: the
 * drop-ins in order, then sddm.conf, the last [Autologin] User= winning.
 * Empty when there is none. */
QString autoLoginUser();

bool store(uid_t uid, const QByteArray &key);
QByteArray load(uid_t uid);
void forget(uid_t uid);
/* Removes every key but the auto sign-in account's, so a key never
 * outlives the setting it was kept for. */
void forgetOthers();
}
