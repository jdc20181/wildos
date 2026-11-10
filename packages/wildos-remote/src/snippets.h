#pragma once

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

// Snippets (saved commands with a name) and each host's command history,
// for the snippets panel and the suggestions under the cursor
// (RemoteTerminal.dc). Both stay on this PC, in ~/.local/share/wildos-remote.
// A line only counts as history once the terminal has shown it back, so
// whatever is typed at a password prompt (which isn't shown) is never
// kept.
class Snippets : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // [{ id, title, command, note }]
    Q_PROPERTY(QVariantList snippets READ snippets NOTIFY changed)

public:
    static Snippets *instance();
    static Snippets *create(QQmlEngine *, QJSEngine *);

    QVariantList snippets() const { return m_snippets; }

    Q_INVOKABLE QString save(const QVariantMap &snippet);
    Q_INVOKABLE void remove(const QString &id);

    void record(const QString &hostKey, const QString &command);
    Q_INVOKABLE void clearHistory(const QString &hostKey);
    // Up to three suggestions for what has been typed so far: the host's
    // most used commands that start with it, then a snippet.
    // [{ kind: "history" | "snippet", text, completion, detail }]
    QVariantList suggest(const QString &hostKey, const QString &typed) const;

Q_SIGNALS:
    void changed();

private:
    explicit Snippets(QObject *parent = nullptr);
    void writeSnippets();
    void writeHistory();

    QVariantList m_snippets;
    // host key -> command -> times used
    QHash<QString, QHash<QString, int>> m_history;
};
