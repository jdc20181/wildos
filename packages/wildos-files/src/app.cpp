#include "app.h"

#include <KWindowSystem>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>

namespace
{
QList<QUrl> toUrls(const QStringList &list)
{
    QList<QUrl> urls;
    for (const QString &text : list) {
        if (text.isEmpty()) {
            continue;
        }
        const QUrl url = QUrl::fromUserInput(text, QDir::currentPath(), QUrl::AssumeLocalFile);
        if (url.isValid()) {
            urls.append(url);
        }
    }
    return urls;
}
}

App *App::instance()
{
    static App *app = new App;
    return app;
}

App *App::create(QQmlEngine *, QJSEngine *)
{
    auto app = instance();
    QJSEngine::setObjectOwnership(app, QJSEngine::CppOwnership);
    return app;
}

App::App(QObject *parent)
    : QObject(parent)
{
}

void App::Open(const QStringList &urls)
{
    openWindow(toUrls(urls));
}

void App::openWindow(const QList<QUrl> &urls)
{
    if (!m_engine) {
        return;
    }
    if (!m_component) {
        m_component = new QQmlComponent(m_engine, QStringLiteral("WildOS.Files"), QStringLiteral("FilesWindow"), this);
    }
    QVariantList list;
    for (const QUrl &url : urls) {
        list.append(url);
    }
    QObject *object = m_component->createWithInitialProperties({{QStringLiteral("startUrls"), list}});
    if (!object) {
        qWarning() << m_component->errors();
        return;
    }
    object->setParent(this);
    if (auto window = qobject_cast<QQuickWindow *>(object)) {
        // The window frees itself when closed; the process stays only while
        // a window is open.
        connect(window, &QQuickWindow::visibleChanged, window, [window] {
            if (!window->isVisible()) {
                window->deleteLater();
            }
        });
        KWindowSystem::updateStartupId(window);
        window->requestActivate();
        KWindowSystem::activateWindow(window);
    }
}

FileManager1::FileManager1(QObject *parent)
    : QObject(parent)
{
}

void FileManager1::ShowFolders(const QStringList &uriList, const QString &)
{
    App::instance()->Open(uriList);
}

void FileManager1::ShowItems(const QStringList &uriList, const QString &)
{
    // Each item is shown selected in its folder, folders included.
    QStringList marked;
    for (const QString &uri : uriList) {
        QUrl url(uri);
        url.setFragment(QStringLiteral("wildos-select"));
        marked.append(url.toString());
    }
    App::instance()->Open(marked);
}

void FileManager1::ShowItemProperties(const QStringList &uriList, const QString &)
{
    App::instance()->Open(uriList);
}
