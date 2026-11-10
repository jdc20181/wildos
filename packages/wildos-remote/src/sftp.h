#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QProcess>

#include <functional>
#include <optional>

// An SFTP (version 3) client over `ssh -s sftp`. It rides on the
// terminal's connection through OpenSSH's connection sharing, so it needs
// no second sign-in and checks nothing ssh hasn't already checked. With no
// terminal signed in, it signs in itself only with a password saved in the
// keyring, and never asks.
class Sftp : public QObject
{
    Q_OBJECT

public:
    struct Entry {
        QString name;
        bool dir = false;
        bool link = false;
        qint64 size = 0;
        qint64 mtime = 0;
        quint32 permissions = 0;
    };
    // An empty error means it worked.
    using Done = std::function<void(const QString &error)>;
    using Listed = std::function<void(const QList<Entry> &entries, const QString &error)>;
    using Progress = std::function<void(qint64 done)>;

    explicit Sftp(QObject *parent = nullptr);
    ~Sftp() override;

    // The ssh arguments that reach the host (control path, port, user@host).
    void start(const QStringList &sshArgs, const std::optional<QString> &password = {});
    void stop();
    bool ready() const { return m_ready; }

    void realPath(const QString &path, const std::function<void(const QString &path, const QString &error)> &done);
    void list(const QString &path, const Listed &done);
    void stat(const QString &path, const std::function<void(const Entry &entry, const QString &error)> &done);
    void mkdir(const QString &path, const Done &done);
    void remove(const QString &path, const Done &done);
    void rmdir(const QString &path, const Done &done);
    void rename(const QString &from, const QString &to, const Done &done);
    void upload(const QString &local, const QString &remote, const Progress &progress, const Done &done);
    void download(const QString &remote, const QString &local, const Progress &progress, const Done &done);

Q_SIGNALS:
    void readyChanged();
    // The connection ended, or never came up: `error` says why.
    void failed(const QString &error);

private:
    using Reply = std::function<void(quint8 type, const QByteArray &body)>;
    void send(quint8 type, const QByteArray &payload, const Reply &reply);
    void readyRead();
    void packet(const QByteArray &data);
    static QString statusText(const QByteArray &body);
    void openHandle(const QString &path, quint32 flags, const std::function<void(const QByteArray &handle, const QString &error)> &done);
    void closeHandle(const QByteArray &handle);

    QProcess *m_process = nullptr;
    QString m_askToken;
    QByteArray m_buffer;
    quint32 m_nextId = 1;
    QHash<quint32, Reply> m_pending;
    bool m_ready = false;
    QString m_stderr;
};
