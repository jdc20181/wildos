#pragma once

#include <QList>
#include <QPointer>
#include <QQmlEngine>
#include <QUrl>
#include <QWidget>

class QQuickWidget;
class QStackedWidget;
class Tab;
class TerminalView;
class WindowFrame;

// A Terminal window. The terminals are widgets (the emulator is one), and
// everything around them, the title bar with its tabs, menus, dialogs and
// the settings, is QML in one transparent layer on top. That layer only
// takes the pointer where it draws: the title bar and the resize edges,
// or the whole window while a menu, a dialog or the settings are open.
class TerminalWindow : public QWidget
{
    Q_OBJECT
    QML_ANONYMOUS

    Q_PROPERTY(QList<QObject *> tabs READ tabs NOTIFY tabsChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(QObject *currentTab READ currentTab NOTIFY currentChanged)
    Q_PROPERTY(bool maximised READ maximised NOTIFY stateChanged)
    Q_PROPERTY(bool fullScreen READ fullScreen NOTIFY stateChanged)
    Q_PROPERTY(bool windowActive READ windowActive NOTIFY stateChanged)
    Q_PROPERTY(bool chromeFull READ chromeFull WRITE setChromeFull NOTIFY chromeFullChanged)
    Q_PROPERTY(bool blurAvailable READ blurAvailable NOTIFY blurAvailableChanged)
    Q_PROPERTY(int terminalTabCount READ terminalTabCount NOTIFY tabsChanged)

public:
    TerminalWindow(const QString &directory, const QStringList &command, QWidget *parent = nullptr);
    ~TerminalWindow() override;

    static constexpr int titleHeight = 42;
    static constexpr int radius = 14;

    QList<QObject *> tabs() const;
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);
    QObject *currentTab() const;
    bool maximised() const;
    bool fullScreen() const;
    bool windowActive() const { return isActiveWindow(); }
    bool chromeFull() const { return m_chromeFull; }
    void setChromeFull(bool full);
    bool blurAvailable() const;
    int terminalTabCount() const;

    Q_INVOKABLE void newTab(const QString &profileId = QString());
    Q_INVOKABLE void newTabAt(int profileIndex);
    Q_INVOKABLE void duplicateTab(int index);
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void closeOtherTabs(int index);
    Q_INVOKABLE void closeTabsToRight(int index);
    Q_INVOKABLE void moveTab(int from, int to);
    Q_INVOKABLE void nextTab(int step);
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void split(bool right);
    Q_INVOKABLE void closePane();
    Q_INVOKABLE void copy();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void openLink();
    Q_INVOKABLE void copyLink();
    Q_INVOKABLE void answerPaste(bool paste);
    Q_INVOKABLE void zoom(int step);
    Q_INVOKABLE void requestClose();
    Q_INVOKABLE void closeNow();
    Q_INVOKABLE void minimise();
    Q_INVOKABLE void toggleMaximised();
    Q_INVOKABLE void toggleFullScreen();
    Q_INVOKABLE void startMove();
    Q_INVOKABLE void startResize(int edges);
    Q_INVOKABLE void focusTerminal();
    Q_INVOKABLE void newWindow();

    // The window size that fits the launch columns and rows in the
    // current font.
    static QSize launchSize();

Q_SIGNALS:
    void tabsChanged();
    void currentChanged();
    void stateChanged();
    void chromeFullChanged();
    void blurAvailableChanged();
    void contextMenuRequested(qreal x, qreal y, bool hasSelection, bool hasLink, bool split);
    void closeConfirmationNeeded(int count);
    void pasteConfirmationNeeded(int lines);
    void profileMenuRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    Tab *tabAt(int index) const;
    TerminalView *currentView() const;
    TerminalView *createView(const QString &profileId, const QString &directory, const QStringList &command);
    void addTab(Tab *tab, int at);
    void removeTab(Tab *tab);
    void watchView(Tab *tab, TerminalView *view);
    void closeView(Tab *tab, TerminalView *view);
    void layoutChildren();
    void updateMask();
    void updateBlur();
    void updateTitle();
    void applyConfig();
    void setupShortcuts();
    void focusNeighbour(Qt::Edge edge);

    QQuickWidget *m_chrome = nullptr;
    QStackedWidget *m_stack = nullptr;
    WindowFrame *m_frame = nullptr;
    QList<Tab *> m_tabs;
    int m_current = -1;
    bool m_chromeFull = false;
    bool m_closing = false;
    QPointer<TerminalView> m_menuView;
    QUrl m_menuLink;
    QPointer<TerminalView> m_pasteView;
    QString m_pasteText;
};
