#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFileInfo>

/* wildos-setup-wallet: opens the KWallet of the account first boot setup
 * made, in that account's first session (wildos-setup-wallet.service).
 *
 * Setup signs the new account in automatically, so pam_kwallet has no
 * password to create the wallet with, and the first app to ask for a
 * secret would get KWallet's own create-a-wallet dialog. A wallet made
 * there with another password never opens at sign-in again. Setup's
 * helper made the key pam_kwallet makes from the password, with the salt
 * pam_kwallet reads; kwalletd creates the wallet with that key here, the
 * same way it does when pam_kwallet hands it one, and every later sign-in
 * with the password opens it.
 */
namespace
{
QDBusMessage kwallet(const QString &method)
{
    return QDBusMessage::createMethodCall(QStringLiteral("org.kde.kwalletd6"), QStringLiteral("/modules/kwalletd6"), QStringLiteral("org.kde.KWallet"),
                                          method);
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wildos-setup-wallet"));

    // Setup leaves the salt and no wallet. Anything else is a later
    // session, or an account setup did not make.
    const QString dir = QDir::homePath() + QStringLiteral("/.local/share/kwalletd");
    if (!QFileInfo::exists(dir + QStringLiteral("/kdewallet.salt")) || QFileInfo::exists(dir + QStringLiteral("/kdewallet.kwl"))) {
        return 0;
    }

    const QDBusMessage take = QDBusMessage::createMethodCall(QStringLiteral("org.wildos.Setup1"), QStringLiteral("/org/wildos/Setup1"),
                                                             QStringLiteral("org.wildos.Setup1"), QStringLiteral("TakeWalletKey"));
    const QDBusReply<QByteArray> key = QDBusConnection::systemBus().call(take, QDBus::Block, 30000);
    if (!key.isValid()) {
        qWarning("no wallet key from setup: %s", qPrintable(key.error().message()));
        return 1;
    }

    QDBusConnection session = QDBusConnection::sessionBus();
    const QDBusReply<QString> localWallet = session.call(kwallet(QStringLiteral("localWallet")), QDBus::Block, 30000);
    const QString wallet = localWallet.isValid() && !localWallet.value().isEmpty() ? localWallet.value() : QStringLiteral("kdewallet");

    // pamOpen never replies. kwalletd handles one connection's calls in
    // order, so isOpen, sent after it, answers once it has run.
    QDBusMessage open = kwallet(QStringLiteral("pamOpen"));
    open << wallet << key.value() << 0;
    if (!session.send(open)) {
        qWarning("could not reach kwalletd");
        return 1;
    }
    QDBusMessage isOpen = kwallet(QStringLiteral("isOpen"));
    isOpen << wallet;
    const QDBusReply<bool> opened = session.call(isOpen, QDBus::Block, 30000);
    if (!opened.isValid() || !opened.value()) {
        qWarning("kwalletd did not open %s with setup's key", qPrintable(wallet));
        return 1;
    }
    return 0;
}
