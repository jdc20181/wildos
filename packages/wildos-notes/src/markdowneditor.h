#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QTimer>
#include <QVariantList>

class QTextBlock;

// One line of Markdown as Notes draws it.
struct MarkdownLine {
    enum Kind { Blank, Paragraph, Heading, Bullet, Check, Ordered, Continuation, Fence, Code, Table, Image, Quote, Rule };
    Kind kind = Paragraph;
    // Characters of leading white space, and the list depth they make.
    int indent = 0;
    int depth = 0;
    // Where the text starts, after the list or heading marker.
    int contentStart = 0;
    int level = 0;
    bool checked = false;
    QString image;
    // What a line under a list item hides in front of its text.
    int reserve = 0;
};

// The block state carried from line to line: inside a code fence, or under
// a list item (its kind and depth), so the next line knows what it is.
namespace MarkdownState
{
MarkdownLine analyse(const QString &text, int previousState, int *nextState);
int listReserve(int depth, MarkdownLine::Kind kind);
// Sets each line's height: text at one and a half times, a blank line
// short, as the board spaces paragraphs and lists. Only block formats
// change, never the text. With join the change rides on the last edit,
// so undo takes both back together. Returns whether anything changed.
bool applySpacing(QTextDocument *doc, qreal scale, bool join);
}

// Styles the Markdown source as it is typed: headings large, checklists and
// bullets drawn as boxes and dots over their hidden marks, emphasis marks
// hidden away from the line being edited. The text itself is never changed
// by drawing it, so the file stays exactly as written.
class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit MarkdownHighlighter(QTextDocument *document);

    QColor ink;
    QColor inkMuted;
    QColor inkSubtle;
    QColor link;
    QColor codeFill;
    QString fontFamily;
    QString monoFamily;
    qreal scale = 1;
    QString baseDir;
    int editingBlock = -1;
    int maxImageWidth = 480;

    struct ImageInfo {
        QString path;
        int width = 0;
        int height = 0;
    };
    ImageInfo imageFor(const QString &destination) const;
    qreal px(qreal size) const
    {
        return size * scale;
    }

protected:
    void highlightBlock(const QString &text) override;

private:
    mutable QHash<QString, ImageInfo> m_images;
};

class MarkdownEditor : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int cursorPosition READ cursorPosition WRITE setCursorPosition NOTIFY cursorPositionChanged)
    Q_PROPERTY(QString baseDir READ baseDir WRITE setBaseDir NOTIFY styleChanged)
    Q_PROPERTY(QColor ink MEMBER m_ink WRITE setInk NOTIFY styleChanged)
    Q_PROPERTY(QColor inkMuted MEMBER m_inkMuted WRITE setInkMuted NOTIFY styleChanged)
    Q_PROPERTY(QColor inkSubtle MEMBER m_inkSubtle WRITE setInkSubtle NOTIFY styleChanged)
    Q_PROPERTY(QColor link MEMBER m_link WRITE setLink NOTIFY styleChanged)
    Q_PROPERTY(QColor codeFill MEMBER m_codeFill WRITE setCodeFill NOTIFY styleChanged)
    Q_PROPERTY(QString fontFamily MEMBER m_fontFamily WRITE setFontFamily NOTIFY styleChanged)
    Q_PROPERTY(qreal scale MEMBER m_scale WRITE setScale NOTIFY styleChanged)
    Q_PROPERTY(int maxImageWidth MEMBER m_maxImageWidth WRITE setMaxImageWidth NOTIFY styleChanged)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY markersChanged)
    Q_PROPERTY(int cursorBlock READ cursorBlock NOTIFY cursorPositionChanged)

public:
    explicit MarkdownEditor(QObject *parent = nullptr);

    QQuickTextDocument *document() const;
    void setDocument(QQuickTextDocument *document);
    int cursorPosition() const;
    void setCursorPosition(int position);
    QString baseDir() const;
    void setBaseDir(const QString &dir);
    void setInk(const QColor &c);
    void setInkMuted(const QColor &c);
    void setInkSubtle(const QColor &c);
    void setLink(const QColor &c);
    void setCodeFill(const QColor &c);
    void setFontFamily(const QString &family);
    void setScale(qreal scale);
    void setMaxImageWidth(int width);
    QVariantList markers() const;
    int cursorBlock() const;

    // The text exactly as typed: no paragraph separators, non-breaking
    // spaces kept.
    Q_INVOKABLE QString text() const;
    // Puts a note's text in the editor, with nothing to undo.
    Q_INVOKABLE void setText(const QString &text);

    // Keeps the cursor out of the marks hidden under a box or bullet. Moving
    // left from the start of an item's text goes on to the line above.
    Q_INVOKABLE int clampCursor(int position, int previous) const;
    // Enter, Shift+Enter and Backspace inside lists; -1 when the key should
    // do what it always does, else where the cursor goes.
    Q_INVOKABLE int handleReturn(int position, bool shift);
    Q_INVOKABLE int handleBackspace(int position);
    Q_INVOKABLE int handleTab(int position, bool back);
    Q_INVOKABLE void toggleCheck(int markerPosition);
    Q_INVOKABLE int toggleHeading(int position);
    Q_INVOKABLE QVariantMap toggleList(int start, int end, const QString &kind);
    Q_INVOKABLE QVariantMap wrap(int start, int end, const QString &open, const QString &close);
    Q_INVOKABLE QVariantMap insertLink(int start, int end);
    Q_INVOKABLE int insertTable(int position);
    // Copies the picture next to the note and links it on a line of its
    // own. Returns where the cursor goes, or -1.
    Q_INVOKABLE int insertImage(int position, const QUrl &file);
    Q_INVOKABLE QString linkAt(int position) const;
    // Undo and redo with the spacing pass held off: the line heights are
    // part of each edit, so stepping back restores them too, and a pass
    // run then would join onto the wrong step.
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    // Formatting at the cursor, for the toolbar's lit buttons.
    Q_INVOKABLE QString lineKind(int position) const;

Q_SIGNALS:
    void documentChanged();
    void cursorPositionChanged();
    void styleChanged();
    void markersChanged();

private:
    QTextDocument *textDocument() const;
    void applyStyle();
    void rebuildMarkers();
    MarkdownLine lineAt(const QTextBlock &block) const;

    QPointer<QQuickTextDocument> m_document;
    MarkdownHighlighter *m_highlighter = nullptr;
    int m_cursorPosition = 0;
    QString m_baseDir;
    QColor m_ink;
    QColor m_inkMuted;
    QColor m_inkSubtle;
    QColor m_link;
    QColor m_codeFill;
    QString m_fontFamily;
    qreal m_scale = 1;
    int m_maxImageWidth = 480;
    QVariantList m_markers;
    QTimer m_markerTimer;
    QTimer m_spacingTimer;
    bool m_spacing = false;
    QTimer m_styleTimer;
};
