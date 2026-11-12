#pragma once

#include <QDBusMessage>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>

#include <functional>

class QSocketNotifier;
class FingerprintAgent;
struct udev;
struct udev_monitor;

// Your fingerprints, through fprintd (net.reactivated.Fprint on the system
// bus), for Settings > Accounts > Sign-in options. fprintd keeps the prints
// in /var/lib/fprint on this PC (or on the reader itself, for readers that
// match on the chip); nothing leaves the computer, and nothing here keeps or
// logs any of it.
//
// Setting up a finger asks for your password first (begin), checks it, and
// answers fprintd's polkit question with it, so the setup dialog is the only
// prompt. Where a finger is taken (sign-in and lock screen, admin prompts)
// is wildos-desktop's /etc/wildos/fingerprint.conf, read here and changed
// through settings-fingerprint.
class Fingerprint : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // A reader fprintd can use.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    // A reader is plugged in, but libfprint lists it as one it can't drive.
    Q_PROPERTY(bool unsupported READ unsupported NOTIFY changed)
    // The reader holds a key Windows Hello set, so Wild OS refuses it.
    Q_PROPERTY(bool windowsKey READ windowsKey NOTIFY changed)
    Q_PROPERTY(QString windowsKeyMessage READ windowsKeyMessage CONSTANT)
    // Where the reader is, as far as the PC says: "built into this PC",
    // "plugged in over USB", or empty when it doesn't say.
    Q_PROPERTY(QString place READ place NOTIFY changed)
    // fprintd's finger names, such as "right-index-finger", in the order
    // they are offered.
    Q_PROPERTY(QStringList fingers READ fingers NOTIFY changed)
    // "", "auth" (checking the password and claiming the reader), "enroll",
    // or the finger being removed.
    Q_PROPERTY(QString busy READ busy NOTIFY busyChanged)
    // Touches the reader has taken for this finger, out of stages
    // (fprintd's enroll-stage-passed and num-enroll-stages).
    Q_PROPERTY(int passed READ passed NOTIFY enrollChanged)
    Q_PROPERTY(int stages READ stages NOTIFY enrollChanged)
    // "press" or "swipe".
    Q_PROPERTY(QString scanType READ scanType NOTIFY changed)
    // Why the last touch didn't count, in plain words; empty once one does.
    Q_PROPERTY(QString retry READ retry NOTIFY enrollChanged)
    // Sign-in options' two switches, from /etc/wildos/fingerprint.conf.
    Q_PROPERTY(bool signIn READ signIn NOTIFY useChanged)
    Q_PROPERTY(bool admin READ admin NOTIFY useChanged)
    // "signin" or "admin" while that switch is being changed.
    Q_PROPERTY(QString switching READ switching NOTIFY useChanged)
    // The setup dialog's print: the path of Fluent UI System Icons'
    // fingerprint_48_regular (48 by 48), read from the vendored SVG.
    Q_PROPERTY(QString print READ print CONSTANT)

public:
    explicit Fingerprint(QObject *parent = nullptr);
    ~Fingerprint() override;

    bool available() const { return !m_device.isEmpty(); }
    bool unsupported() const { return m_unsupported; }
    bool windowsKey() const { return m_windowsKey; }
    QString windowsKeyMessage() const;
    QString place() const { return m_place; }
    QStringList fingers() const { return m_fingers; }
    QString busy() const { return m_busy; }
    int passed() const { return m_passed; }
    int stages() const { return m_stages; }
    QString scanType() const { return m_scanType; }
    QString retry() const { return m_retry; }
    bool signIn() const { return m_signIn; }
    bool admin() const { return m_admin; }
    QString switching() const { return m_switching; }
    QString print() const;

    Q_INVOKABLE void reload();
    // Step one of the setup dialog: checks the password, claims the reader
    // and starts on the first finger not set up yet. started() or
    // failed("password" | "setup", ...) follows.
    Q_INVOKABLE void begin(const QString &password);
    // The next finger on the reader already claimed: "Add another finger",
    // or "Try again" after a finger that wasn't saved.
    Q_INVOKABLE void again();
    // Lets go of the reader: Done, Cancel or the dialog closing.
    Q_INVOKABLE void finish();
    Q_INVOKABLE void remove(const QString &finger);
    // what is "signin" or "admin".
    Q_INVOKABLE void setUse(const QString &what, bool on);

Q_SIGNALS:
    void changed();
    void busyChanged();
    void enrollChanged();
    void useChanged();
    void started();
    void enrolled(const QString &finger);
    // what: "password" (step one's password was wrong), "setup" (the
    // reader couldn't be started), "enroll" (a finger wasn't saved),
    // "remove" or "use".
    void failed(const QString &what, const QString &reason);

private Q_SLOTS:
    void onEnrollStatus(const QString &result, bool done);

private:
    QDBusMessage deviceCall(const QString &method, const QVariantList &arguments = {}) const;
    void claim(const std::function<void()> &next, const QString &what);
    void startEnroll();
    void stopEnroll(const std::function<void()> &next);
    void release();
    void readFingers();
    void readUse();
    void watch();
    void onUdev();
    void setBusy(const QString &busy);
    void dropAgent();
    QString nextFinger() const;
    QString interrupted(const QString &what, const QString &errorName) const;

    QString m_device;
    bool m_unsupported = false;
    bool m_windowsKey = false;
    QString m_place;
    QStringList m_fingers;
    QString m_busy;
    QString m_finger;
    bool m_claimed = false;
    // Counts setups and removals, so an answer that lands after the dialog
    // closed lets the reader go instead of carrying on.
    int m_attempt = 0;
    bool m_enrolling = false;
    int m_passed = 0;
    int m_stages = 0;
    QString m_scanType = QStringLiteral("press");
    QString m_retry;
    bool m_signIn = true;
    bool m_admin = true;
    QString m_switching;
    QPointer<FingerprintAgent> m_agent;

    bool m_watching = false;
    struct udev *m_udev = nullptr;
    struct udev_monitor *m_monitor = nullptr;
    QSocketNotifier *m_udevNotifier = nullptr;
    QTimer m_settle;
};
