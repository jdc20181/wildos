#include "voiceapp.h"

#include <KWindowSystem>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace
{
VoiceApp *s_instance = nullptr;
}

VoiceApp::VoiceApp(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

VoiceApp *VoiceApp::instance()
{
    if (!s_instance) {
        new VoiceApp(qApp);
    }
    return s_instance;
}

VoiceApp *VoiceApp::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

void VoiceApp::handle(const QStringList &arguments, const QString &workingDirectory)
{
    // arguments[0] is the program, as KDBusService passes them.
    for (qsizetype i = 1; i < arguments.size(); ++i) {
        const QString &argument = arguments.at(i);
        if (argument.startsWith(QLatin1Char('-'))) {
            continue;
        }
        const QUrl url = QUrl::fromUserInput(argument, workingDirectory, QUrl::AssumeLocalFile);
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile()) {
            m_pendingPath = QFileInfo(url.toLocalFile()).absoluteFilePath();
            Q_EMIT pendingPathChanged();
            break;
        }
    }
    showMain();
}

void VoiceApp::takePendingPath()
{
    if (!m_pendingPath.isEmpty()) {
        m_pendingPath.clear();
        Q_EMIT pendingPathChanged();
    }
}

void VoiceApp::showMain()
{
    if (!m_window) {
        return;
    }
    m_window->show();
    if (m_window->visibility() == QWindow::Minimized) {
        m_window->showNormal();
    }
    m_window->raise();
    // A start from the launcher carries an activation token, which lets the
    // window take the focus from the app in front.
    KWindowSystem::updateStartupId(m_window);
    KWindowSystem::activateWindow(m_window);
    m_window->requestActivate();
}
