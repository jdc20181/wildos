#include "selftest.h"
#include "client.h"
#include "rail.h"
#include "railwindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>

#include <memory>
#include <utility>

namespace
{
// US positions of the characters the test types, as Linux key codes.
int evdevFor(QChar c, bool *shift)
{
    static const char *rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    static const int starts[] = {16, 30, 44};
    *shift = c.isUpper();
    const char l = char(c.toLower().unicode());
    for (int r = 0; r < 3; ++r) {
        for (int i = 0; rows[r][i]; ++i) {
            if (rows[r][i] == l) {
                return starts[r] + i;
            }
        }
    }
    if (l >= '1' && l <= '9') {
        return 2 + (l - '1');
    }
    switch (l) {
    case '0': return 11;
    case ' ': return 57;
    case '\n': return 28;
    case '.': return 52;
    case ',': return 51;
    case '-': return 12;
    default: return 0;
    }
}

void key(QWindow *w, int evdev, bool down)
{
    QKeyEvent e(down ? QEvent::KeyPress : QEvent::KeyRelease, 0, Qt::NoModifier, quint32(evdev + 8), 0, 0);
    QCoreApplication::sendEvent(w, &e);
}
}

void startSelfTest(Rail *rail, Client *client)
{
    const QString text = qEnvironmentVariable("WILDOS_RDP_TEST_TYPE");
    const QString click = qEnvironmentVariable("WILDOS_RDP_TEST_CLICK");
    const QString dump = qEnvironmentVariable("WILDOS_RDP_TEST_DUMP");
    const int quit = qEnvironmentVariableIntValue("WILDOS_RDP_TEST_QUIT");
    if (text.isEmpty() && click.isEmpty() && dump.isEmpty()) {
        return;
    }
    if (!dump.isEmpty()) {
        // The loading window every quarter of a second while it is up, at
        // a 4K laptop's 2.25 scale.
        QObject::connect(rail, &Rail::loadingVisible, rail, [=](quint32 token) {
            QDir().mkpath(dump);
            auto *frames = new QTimer(rail);
            auto count = std::make_shared<int>(0);
            QObject::connect(frames, &QTimer::timeout, rail, [=] {
                RailWindow *w = rail->loadingWindow(token);
                if (!w || !w->loading() || ++*count > 120) {
                    frames->deleteLater();
                    return;
                }
                w->loadingSnapshot(1.5).save(dump + QStringLiteral("/loading-%1-%2.png").arg(token).arg(*count, 3, 10, QLatin1Char('0')));
            });
            frames->start(250);
        });
    }
    QObject::connect(rail, &Rail::windowShown, rail, [=](quint32 id) {
        static bool started = false;
        if (started) {
            return;
        }
        started = true;
        // Long enough for the app to finish drawing and take the keyboard.
        QTimer::singleShot(2500, rail, [=] {
            QPointer<RailWindow> w = rail->window(id);
            if (!w) {
                return;
            }
            if (!click.isEmpty()) {
                const QPointF at(click.section(u',', 0, 0).toDouble(), click.section(u',', 1, 1).toDouble());
                QMouseEvent press(QEvent::MouseButtonPress, at, w->mapToGlobal(at), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QCoreApplication::sendEvent(w, &press);
                QMouseEvent release(QEvent::MouseButtonRelease, at, w->mapToGlobal(at), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(w, &release);
            }
            // At a quick typist's pace, one key every 30 ms; <ctrl+c> is a
            // chord with Ctrl, <wait> a pause of half a second.
            int at = 0;
            for (qsizetype i = 0; i < text.size(); ++i) {
                bool shift = false;
                bool ctrl = false;
                int code = 0;
                if (text[i] == u'<' && text.indexOf(u'>', i) > i) {
                    const qsizetype end = text.indexOf(u'>', i);
                    const QString token = text.mid(i + 1, end - i - 1);
                    i = end;
                    if (token == QLatin1String("wait")) {
                        at += 16;
                        continue;
                    }
                    ctrl = token.startsWith(QLatin1String("ctrl+"));
                    code = evdevFor(token.back(), &shift);
                } else {
                    code = evdevFor(text[i], &shift);
                }
                if (!code) {
                    continue;
                }
                QTimer::singleShot(30 * at++, rail, [w, code, shift, ctrl] {
                    if (!w) {
                        return;
                    }
                    for (const auto &[held, mod] : {std::pair{ctrl, 29}, std::pair{shift, 42}}) {
                        if (held) {
                            key(w, mod, true);
                        }
                    }
                    key(w, code, true);
                    key(w, code, false);
                    for (const auto &[held, mod] : {std::pair{shift, 42}, std::pair{ctrl, 29}}) {
                        if (held) {
                            key(w, mod, false);
                        }
                    }
                });
            }
            QTimer::singleShot(1500 + 30 * at, rail, [=] {
                if (!dump.isEmpty()) {
                    QDir().mkpath(dump);
                    for (quint32 other : rail->windowIds()) {
                        if (RailWindow *rw = rail->window(other); rw && rw->isVisible()) {
                            const QImage image = rw->snapshot();
                            if (!image.isNull()) {
                                image.save(dump + QStringLiteral("/%1.png").arg(other, 8, 16, QLatin1Char('0')));
                            }
                        }
                    }
                    qInfo("wildos-rdp: test pictures in %s", qPrintable(dump));
                }
                if (quit > 0) {
                    QTimer::singleShot(quit * 1000, rail, [client] { client->stop(); });
                }
            });
        });
    });
}
