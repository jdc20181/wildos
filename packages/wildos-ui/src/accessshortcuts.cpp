#include "accessshortcuts.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QProcess>
#include <QStandardPaths>
#include <QWindow>

AccessShortcuts::AccessShortcuts(QObject *parent)
    : QObject(parent)
{
    qGuiApp->installEventFilter(this);
}

bool AccessShortcuts::keyboardOn() const
{
    return KSharedConfig::openConfig(QStringLiteral("wildosrc"))->group(QStringLiteral("Accessibility")).readEntry("OnScreenKeyboard", false);
}

void AccessShortcuts::restore() const
{
    run({QStringLiteral("restore")});
}

void AccessShortcuts::run(const QStringList &arguments) const
{
    const QString program = QStandardPaths::findExecutable(QStringLiteral("wildos-access"));
    if (!program.isEmpty()) {
        QProcess::startDetached(program, arguments);
    }
}

bool AccessShortcuts::eventFilter(QObject *watched, QEvent *event)
{
    // A key event is sent to the window and then on to each item in turn;
    // the window sees it once.
    if ((event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) || !qobject_cast<QWindow *>(watched)) {
        return QObject::eventFilter(watched, event);
    }
    const auto key = static_cast<QKeyEvent *>(event);
    if (key->isAutoRepeat()) {
        return false;
    }
    const Qt::KeyboardModifiers modifiers = key->modifiers() & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);

    if (event->type() == QEvent::KeyRelease) {
        if (key->key() == Qt::Key_Shift && m_shiftAlone) {
            // Five taps of Shift on its own, each within a second of the
            // last, as on Windows.
            if (!m_lastShift.isValid() || m_lastShift.elapsed() > 1000) {
                m_shiftTaps = 0;
            }
            m_lastShift.restart();
            if (++m_shiftTaps == 5) {
                m_shiftTaps = 0;
                run({QStringLiteral("toggle"), QStringLiteral("sticky-keys")});
            }
        }
        m_shiftAlone = false;
        return false;
    }

    if (key->key() == Qt::Key_Shift) {
        m_shiftAlone = (modifiers & ~Qt::ShiftModifier) == Qt::NoModifier;
        return false;
    }
    m_shiftAlone = false;
    m_shiftTaps = 0;

    const Qt::KeyboardModifiers superAlt = Qt::MetaModifier | Qt::AltModifier;
    const Qt::KeyboardModifiers superCtrl = Qt::MetaModifier | Qt::ControlModifier;
    if (modifiers == superAlt && key->key() == Qt::Key_S) {
        run({QStringLiteral("toggle"), QStringLiteral("screen-reader")});
        return true;
    }
    if (modifiers == superAlt && key->key() == Qt::Key_H) {
        run({QStringLiteral("toggle"), QStringLiteral("high-contrast")});
        return true;
    }
    if (modifiers == superCtrl && key->key() == Qt::Key_O) {
        Q_EMIT keyboardRequested();
        return true;
    }
    // Plus is Shift and = on most layouts, so Shift is allowed with it.
    if (m_magnifier && (modifiers & ~Qt::ShiftModifier) == Qt::MetaModifier) {
        switch (key->key()) {
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            run({QStringLiteral("zoom"), QStringLiteral("in")});
            return true;
        case Qt::Key_Minus:
            run({QStringLiteral("zoom"), QStringLiteral("out")});
            return true;
        case Qt::Key_0:
            run({QStringLiteral("zoom"), QStringLiteral("reset")});
            return true;
        default:
            break;
        }
    }
    return false;
}
