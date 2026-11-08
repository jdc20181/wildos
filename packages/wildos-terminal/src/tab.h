#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>

#include "terminalview.h"

class QWidget;

// One tab of a window: a terminal (split into panes or not) or the
// settings, which open as a tab as they do in Windows Terminal.
class Tab : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)
    Q_PROPERTY(bool isSettings READ isSettings CONSTANT)
    Q_PROPERTY(int paneCount READ paneCount NOTIFY changed)

public:
    Tab(bool settings, QWidget *page, QObject *parent = nullptr);

    QString title() const;
    QString iconName() const;
    bool isSettings() const { return m_settings; }
    int paneCount() const { return m_views.size(); }

    QWidget *page() const { return m_page; }
    TerminalView *current() const { return m_current; }
    const QList<TerminalView *> &views() const { return m_views; }

    void addFirst(TerminalView *view);
    // Puts a new pane beside (Qt::Horizontal) or under (Qt::Vertical) the
    // current one, sharing its space equally.
    void split(TerminalView *view, Qt::Orientation orientation);
    // Takes a pane out; true when it was the last one.
    bool remove(TerminalView *view);
    void setCurrent(TerminalView *view);
    // The pane next to the current one in a direction, by position.
    TerminalView *neighbour(Qt::Edge edge) const;

Q_SIGNALS:
    void changed();
    void viewAdded(TerminalView *view);

private:
    void track(TerminalView *view);
    void updateOutlines();

    bool m_settings;
    QWidget *m_page;
    QList<TerminalView *> m_views;
    QPointer<TerminalView> m_current;
};
