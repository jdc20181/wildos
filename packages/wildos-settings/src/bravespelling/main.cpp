// wildos-brave-spelling: hands the languages chosen in Settings > Typing to
// Brave, through the root helper settings-brave-spelling. Settings runs it
// when the spelling language changes, and every session starts it once, so
// an update, a new account or a new display language (which Sonnet follows
// until a spelling language is picked) reaches Brave too. It asks only when
// the languages differ from the ones it last handed over, so a normal login
// does nothing.

#include <Sonnet/Settings>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Sonnet checks in the other languages only while "Detect the language
    // I'm writing in" is on; Brave checks every language it has at once.
    Sonnet::Settings sonnet;
    QStringList languages{sonnet.defaultLanguage()};
    if (sonnet.autodetectLanguage()) {
        for (const QString &language : sonnet.preferredLanguages()) {
            if (!languages.contains(language)) {
                languages << language;
            }
        }
    }
    languages.removeAll(QString());
    if (languages.isEmpty()) {
        return 0;
    }

    const QString stateDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/wildos-settings");
    const QString stateFile = stateDir + QStringLiteral("/brave-spelling");
    const QByteArray wanted = languages.join(QLatin1Char(' ')).toUtf8();
    QFile state(stateFile);
    if (state.open(QIODevice::ReadOnly) && state.readAll().trimmed() == wanted) {
        return 0;
    }
    state.close();

    // Polkit lets the person at the computer do this without a password.
    if (QProcess::execute(QStringLiteral("pkexec"), QStringList{QStringLiteral("/usr/libexec/wildos/settings-brave-spelling")} + languages) != 0) {
        return 1;
    }
    QDir().mkpath(stateDir);
    QSaveFile out(stateFile);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(wanted + '\n');
        out.commit();
    }
    return 0;
}
