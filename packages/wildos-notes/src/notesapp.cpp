#include "notesapp.h"

#include <KWindowSystem>

#include <QCoreApplication>
#include <QDebug>
#include <QQmlComponent>

namespace
{
NotesApp *s_instance = nullptr;
}

NotesApp::NotesApp(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

NotesApp *NotesApp::instance()
{
    if (!s_instance) {
        new NotesApp(QCoreApplication::instance());
    }
    return s_instance;
}

NotesApp *NotesApp::create(QQmlEngine *, QJSEngine *)
{
    NotesApp *app = instance();
    QQmlEngine::setObjectOwnership(app, QQmlEngine::CppOwnership);
    return app;
}

void NotesApp::setEngine(QQmlEngine *engine)
{
    m_engine = engine;
}

void NotesApp::setMainWindow(QQuickWindow *window)
{
    m_main = window;
}

void NotesApp::handle(const QStringList &arguments)
{
    if (arguments.contains(QStringLiteral("--quick"))) {
        quickNote();
    } else {
        openNote(QString());
    }
}

void NotesApp::openNote(const QString &path)
{
    if (!m_main && m_engine) {
        QQmlComponent component(m_engine, QStringLiteral("WildOS.Notes"), QStringLiteral("Main"));
        QObject *object = component.create();
        if (!object) {
            qWarning().noquote() << component.errorString();
            return;
        }
        QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
        object->setParent(m_engine);
        m_main = qobject_cast<QQuickWindow *>(object);
    }
    if (!m_main) {
        return;
    }
    if (!path.isEmpty()) {
        QMetaObject::invokeMethod(m_main, "openPath", Q_ARG(QVariant, path));
    }
    activate(m_main);
}

void NotesApp::quickNote()
{
    if (m_quick && m_quick->isVisible()) {
        activate(m_quick);
        return;
    }
    if (!m_engine) {
        return;
    }
    QQmlComponent component(m_engine, QStringLiteral("WildOS.Notes"), QStringLiteral("QuickNote"));
    QObject *object = component.create();
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        qWarning().noquote() << component.errorString();
        delete object;
        return;
    }
    QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
    // Each quick note window goes once closed; its note was saved as it
    // closed.
    connect(window, &QQuickWindow::closing, window, &QObject::deleteLater);
    m_quick = window;
    activate(window);
}

void NotesApp::activate(QQuickWindow *window)
{
    window->show();
    window->raise();
    // A start from the shortcut or the launcher carries an activation token,
    // which lets the window take the focus from the app in front.
    KWindowSystem::updateStartupId(window);
    KWindowSystem::activateWindow(window);
    window->requestActivate();
}
