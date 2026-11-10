#pragma once

#include <QPointer>
#include <QProcessEnvironment>
#include <QVariantList>
#include <qtermwidget.h>

// One terminal: QTermWidget, the emulator Wild OS Terminal is built on,
// running ssh (or the person's shell for a local tab), with suggestions
// layered on top (RemoteTerminal.dc).
//
// Remote can't see the shell's own line, so it follows what is typed since
// the last Enter. Anything that moves the cursor or edits the line in ways
// it can't follow (arrows, Tab completion, Ctrl+R) puts suggestions away
// until the next line. A line counts as history only when the terminal
// shows it back, which a password prompt never does.
class TerminalView : public QTermWidget
{
    Q_OBJECT

public:
    // `environment` is what the program runs with, before the terminal's own
    // settings (TERM and the like) go on top.
    TerminalView(const QString &program, const QStringList &args, const QString &historyKey,
                 QProcessEnvironment environment = QProcessEnvironment::systemEnvironment(), QWidget *parent = nullptr);

    QVariantList suggestions() const { return m_suggestions; }
    int selected() const { return m_selected; }
    // Where the cursor is, in this widget's coordinates.
    QRect cursorRect() const;

    void accept(int index);
    void dismiss();
    void setSelected(int index);
    // Types a command and presses Enter.
    void run(const QString &command);
    void copy();
    void paste();
    bool exited() const { return m_exited; }

Q_SIGNALS:
    void suggestionsChanged();
    void focused();
    void exitedChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *display() const;
    void updateSuggestions();
    void received(const QString &text);
    void lineEntered();

    QString m_historyKey;
    QPointer<QWidget> m_display;
    QString m_line;
    bool m_lineValid = true;
    // What the terminal has shown, escapes removed, the oldest dropped as it
    // grows; positions are counted from the start of the session.
    QString m_echo;
    qint64 m_echoDropped = 0;
    qint64 m_lineStart = 0;
    QVariantList m_suggestions;
    int m_selected = 0;
    bool m_hidden = false;
    bool m_exited = false;
};
