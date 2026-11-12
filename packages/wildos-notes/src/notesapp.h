#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>

// The windows of the one Notes process: the main window and quick notes.
// Super+N starts Notes with --quick, or asks the running one for a quick
// note, so a second process never runs.
class NotesApp : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // No default constructor: QML would make a second one instead of
    // calling create().
    explicit NotesApp(QObject *parent);
    static NotesApp *instance();
    static NotesApp *create(QQmlEngine *, QJSEngine *);

    void setEngine(QQmlEngine *engine);
    void setMainWindow(QQuickWindow *window);
    // Acts on a command line: from this start, or handed over by a second
    // start while Notes is running.
    void handle(const QStringList &arguments);

    // Brings up the main window (making it if only quick notes are open)
    // with the note at path selected.
    Q_INVOKABLE void openNote(const QString &path);
    Q_INVOKABLE void quickNote();

private:
    void activate(QQuickWindow *window);

    QQmlEngine *m_engine = nullptr;
    QPointer<QQuickWindow> m_main;
    QPointer<QQuickWindow> m_quick;
};
