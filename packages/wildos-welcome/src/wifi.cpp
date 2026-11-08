#include "wifi.h"

#include <QHash>
#include <QProcess>

#include <algorithm>

Wifi::Wifi(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(4000);
    connect(&m_timer, &QTimer::timeout, this, &Wifi::poll);
    m_timer.start();
    QTimer::singleShot(0, this, &Wifi::poll);
}

QStringList Wifi::splitTerse(const QString &line)
{
    QStringList fields;
    QString field;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == u'\\' && i + 1 < line.size()) {
            field += line.at(++i);
        } else if (c == u':') {
            fields << field;
            field.clear();
        } else {
            field += c;
        }
    }
    fields << field;
    return fields;
}

void Wifi::run(const QStringList &args, Done done, int timeoutMs, const QByteArray &input)
{
    auto *p = new QProcess(this);
    auto *timeout = new QTimer(p);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, p, &QProcess::kill);
    connect(p, &QProcess::finished, this, [p, done](int code, QProcess::ExitStatus status) {
        const QString out = QString::fromUtf8(p->readAllStandardOutput());
        const QString err = QString::fromUtf8(p->readAllStandardError());
        p->deleteLater();
        done(status == QProcess::NormalExit ? code : -1, out, err);
    });
    connect(p, &QProcess::errorOccurred, this, [p, done](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            p->deleteLater();
            done(-1, QString(), QStringLiteral("nmcli is missing"));
        }
    });
    // A fixed C locale keeps nmcli's states and messages in English for parsing.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    p->setProcessEnvironment(env);
    p->start(QStringLiteral("nmcli"), args);
    if (!input.isEmpty()) {
        p->write(input);
    }
    p->closeWriteChannel();
    timeout->start(timeoutMs);
}

void Wifi::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    Q_EMIT activeChanged();
    if (active) {
        rescan();
    }
}

void Wifi::poll()
{
    if (m_polling) {
        return;
    }
    m_polling = true;
    run({QStringLiteral("-t"), QStringLiteral("-e"), QStringLiteral("yes"), QStringLiteral("-f"), QStringLiteral("TYPE,STATE"), QStringLiteral("device")},
        [this](int code, const QString &out, const QString &) {
            if (code == 0) {
                readDevices(out);
            }
            run({QStringLiteral("-t"), QStringLiteral("radio"), QStringLiteral("wifi")}, [this](int, const QString &radio, const QString &) {
                m_enabled = radio.trimmed() == QLatin1String("enabled");
                run({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("CONNECTIVITY"), QStringLiteral("general")},
                    [this](int, const QString &state, const QString &) {
                        m_online = state.trimmed() == QLatin1String("full");
                        m_ready = true;
                        m_polling = false;
                        Q_EMIT changed();
                        if (m_active && m_available && m_enabled) {
                            listNetworks();
                        }
                    });
            });
        });
}

void Wifi::readDevices(const QString &out)
{
    bool available = false;
    bool wired = false;
    const auto lines = out.split(u'\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList f = splitTerse(line);
        if (f.size() < 2) {
            continue;
        }
        if (f[0] == QLatin1String("wifi")) {
            available = true;
        }
        if (f[0] == QLatin1String("ethernet") && f[1].startsWith(QLatin1String("connected"))) {
            wired = true;
        }
    }
    m_available = available;
    m_wiredOnline = wired;
}

void Wifi::rescan()
{
    if (!m_available || !m_enabled) {
        poll();
        return;
    }
    m_scanning = true;
    Q_EMIT changed();
    run({QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("rescan")}, [this](int, const QString &, const QString &) {
        // NetworkManager answers before the scan is done; the list fills in
        // over the next few seconds of polling.
        QTimer::singleShot(2500, this, [this] {
            m_scanning = false;
            Q_EMIT changed();
            listNetworks();
        });
    });
}

void Wifi::listNetworks()
{
    run({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME,TYPE"), QStringLiteral("connection"), QStringLiteral("show")},
        [this](int, const QString &out, const QString &) {
            m_saved.clear();
            for (const QString &line : out.split(u'\n', Qt::SkipEmptyParts)) {
                const QStringList f = splitTerse(line);
                if (f.size() >= 2 && f[1] == QLatin1String("802-11-wireless")) {
                    m_saved << f[0];
                }
            }
            run({QStringLiteral("-t"), QStringLiteral("-e"), QStringLiteral("yes"), QStringLiteral("-f"), QStringLiteral("IN-USE,SSID,SIGNAL,SECURITY"),
                 QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("list"), QStringLiteral("--rescan"), QStringLiteral("no")},
                [this](int code, const QString &out, const QString &) {
                    if (code != 0) {
                        return;
                    }
                    QHash<QString, QVariantMap> bySsid;
                    QString connected;
                    for (const QString &line : out.split(u'\n', Qt::SkipEmptyParts)) {
                        const QStringList f = splitTerse(line);
                        if (f.size() < 4 || f[1].isEmpty()) {
                            continue;
                        }
                        const bool inUse = f[0].trimmed() == QLatin1String("*");
                        const int signal = f[2].toInt();
                        const QString security = f[3].trimmed();
                        if (inUse) {
                            connected = f[1];
                        }
                        QVariantMap &n = bySsid[f[1]];
                        if (n.isEmpty() || n.value(QStringLiteral("signal")).toInt() < signal) {
                            n[QStringLiteral("ssid")] = f[1];
                            n[QStringLiteral("signal")] = signal;
                            n[QStringLiteral("secure")] = !security.isEmpty() && security != QLatin1String("--");
                            n[QStringLiteral("enterprise")] = security.contains(QLatin1String("802.1X"));
                            n[QStringLiteral("security")] = security;
                            n[QStringLiteral("saved")] = m_saved.contains(f[1]);
                        }
                        if (inUse) {
                            n[QStringLiteral("active")] = true;
                        }
                    }
                    QList<QVariantMap> list = bySsid.values();
                    std::sort(list.begin(), list.end(), [](const QVariantMap &a, const QVariantMap &b) {
                        const bool aa = a.value(QStringLiteral("active")).toBool();
                        const bool ba = b.value(QStringLiteral("active")).toBool();
                        if (aa != ba) {
                            return aa;
                        }
                        return a.value(QStringLiteral("signal")).toInt() > b.value(QStringLiteral("signal")).toInt();
                    });
                    m_networks.clear();
                    for (const QVariantMap &n : list) {
                        m_networks << n;
                    }
                    if (m_connectedSsid != connected) {
                        m_connectedSsid = connected;
                        Q_EMIT changed();
                    }
                    Q_EMIT networksChanged();
                });
        });
}

void Wifi::connectTo(const QString &ssid, const QString &password)
{
    if (!m_connectingSsid.isEmpty()) {
        return;
    }
    m_connectingSsid = ssid;
    Q_EMIT changed();

    const bool saved = m_saved.contains(ssid);
    const QStringList wait{QStringLiteral("--wait"), QStringLiteral("40")};
    if (password.isEmpty()) {
        // A saved network, or an open one.
        const QStringList args = saved ? wait + QStringList{QStringLiteral("connection"), QStringLiteral("up"), QStringLiteral("id"), ssid}
                                       : wait + QStringList{QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("connect"), ssid};
        run(args, [this, ssid](int code, const QString &out, const QString &err) {
            finishConnect(ssid, code, out + err, false);
        }, 45000);
        return;
    }

    // The password goes to nmcli on its standard input, never on its command
    // line, where every process on the computer could read it: the profile
    // is made without it, nmcli's editor sets it from stdin, then it starts.
    QString keyMgmt = QStringLiteral("wpa-psk");
    for (const QVariant &n : std::as_const(m_networks)) {
        const QVariantMap net = n.toMap();
        const QString security = net.value(QStringLiteral("security")).toString();
        if (net.value(QStringLiteral("ssid")).toString() == ssid && security.contains(QLatin1String("WPA3")) && !security.contains(QLatin1String("WPA2"))
            && !security.contains(QLatin1String("WPA1"))) {
            keyMgmt = QStringLiteral("sae");
        }
    }
    if (saved) {
        run({QStringLiteral("connection"), QStringLiteral("delete"), QStringLiteral("id"), ssid}, [](int, const QString &, const QString &) {});
    }
    addProfile(ssid, password, keyMgmt, false);
}

void Wifi::connectHidden(const QString &ssid, const QString &password)
{
    if (!m_connectingSsid.isEmpty() || ssid.trimmed().isEmpty()) {
        return;
    }
    m_connectingSsid = ssid;
    Q_EMIT changed();
    if (m_saved.contains(ssid)) {
        run({QStringLiteral("connection"), QStringLiteral("delete"), QStringLiteral("id"), ssid}, [](int, const QString &, const QString &) {});
    }
    addProfile(ssid, password, password.isEmpty() ? QString() : QStringLiteral("wpa-psk"), true);
}

void Wifi::addProfile(const QString &ssid, const QString &password, const QString &keyMgmt, bool hidden)
{
    const QStringList wait{QStringLiteral("--wait"), QStringLiteral("40")};
    QStringList args{QStringLiteral("connection"), QStringLiteral("add"), QStringLiteral("type"), QStringLiteral("wifi"), QStringLiteral("ifname"), QStringLiteral("*"),
                     QStringLiteral("con-name"), ssid, QStringLiteral("ssid"), ssid};
    if (hidden) {
        args << QStringLiteral("802-11-wireless.hidden") << QStringLiteral("yes");
    }
    // psk-flags 0: the password is kept with the connection by the system,
    // so it still works once the setup account is gone.
    if (!keyMgmt.isEmpty()) {
        args << QStringLiteral("wifi-sec.key-mgmt") << keyMgmt << QStringLiteral("wifi-sec.psk-flags") << QStringLiteral("0");
    }
    run(args, [this, ssid, password, wait](int code, const QString &out, const QString &err) {
        if (code != 0) {
            finishConnect(ssid, code, out + err, true);
            return;
        }
        const auto up = [this, ssid, wait] {
            run(wait + QStringList{QStringLiteral("connection"), QStringLiteral("up"), QStringLiteral("id"), ssid},
                [this, ssid](int code, const QString &out, const QString &err) {
                    finishConnect(ssid, code, out + err, true);
                },
                45000);
        };
        if (password.isEmpty()) {
            up();
            return;
        }
        const QByteArray script = "set 802-11-wireless-security.psk " + password.toUtf8() + "\nsave persistent\nquit\n";
        run({QStringLiteral("connection"), QStringLiteral("edit"), QStringLiteral("id"), ssid},
            [this, ssid, up](int code, const QString &out, const QString &err) {
                if (code != 0) {
                    finishConnect(ssid, code, out + err, true);
                    return;
                }
                up();
            },
            15000, script);
    });
}

void Wifi::finishConnect(const QString &ssid, int code, const QString &text, bool madeProfile)
{
    m_connectingSsid.clear();
    QString reason;
    if (code != 0) {
        if (text.contains(QLatin1String("Secrets were required")) || text.contains(QLatin1String("psk"))
            || text.contains(QLatin1String("password"), Qt::CaseInsensitive)) {
            reason = QStringLiteral("password");
        } else if (text.contains(QLatin1String("No network with SSID"))) {
            reason = QStringLiteral("range");
        } else {
            reason = QStringLiteral("other");
        }
        // A profile that never worked would keep a wrong password.
        if (madeProfile) {
            run({QStringLiteral("connection"), QStringLiteral("delete"), QStringLiteral("id"), ssid}, [](int, const QString &, const QString &) {});
        }
    }
    Q_EMIT changed();
    Q_EMIT connectFinished(ssid, code == 0, reason);
    poll();
    listNetworks();
}

void Wifi::setRadio(bool on)
{
    run({QStringLiteral("radio"), QStringLiteral("wifi"), on ? QStringLiteral("on") : QStringLiteral("off")}, [this](int, const QString &, const QString &) {
        poll();
    });
}

void Wifi::setAutoConnect(const QString &ssid, bool on)
{
    run({QStringLiteral("connection"), QStringLiteral("modify"), QStringLiteral("id"), ssid, QStringLiteral("connection.autoconnect"),
         on ? QStringLiteral("yes") : QStringLiteral("no")},
        [](int, const QString &, const QString &) {});
}
