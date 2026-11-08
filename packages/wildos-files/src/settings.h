#pragma once

#include <KSharedConfig>
#include <QSize>
#include <QObject>
#include <QQmlEngine>
#include <QUrl>

// View options the user changes from the View menu, kept between sessions
// in wildosfilesrc.
class Settings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool showExtensions READ showExtensions WRITE setShowExtensions NOTIFY showExtensionsChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(bool detailsPane READ detailsPane WRITE setDetailsPane NOTIFY detailsPaneChanged)
    Q_PROPERTY(QList<qreal> columnWeights READ columnWeights WRITE setColumnWeights NOTIFY columnWeightsChanged)

public:
    static Settings *instance();
    static Settings *create(QQmlEngine *, QJSEngine *);

    bool showExtensions() const { return m_showExtensions; }
    void setShowExtensions(bool on);
    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool on);
    bool detailsPane() const { return m_detailsPane; }
    void setDetailsPane(bool on);
    QList<qreal> columnWeights() const { return m_columnWeights; }
    void setColumnWeights(const QList<qreal> &weights);

    // "xlarge", "large", "icons" (medium), "small", "list", "details",
    // "tiles" or "content", remembered for each folder the way
    // Windows does, with medium icons for folders never changed.
    Q_INVOKABLE QString viewFor(const QUrl &url) const;
    Q_INVOKABLE void setViewFor(const QUrl &url, const QString &mode);
    // New windows open at the size the last one was closed at.
    Q_INVOKABLE QSize windowSize() const;
    Q_INVOKABLE void setWindowSize(const QSize &size);

Q_SIGNALS:
    void showExtensionsChanged();
    void showHiddenChanged();
    void detailsPaneChanged();
    void columnWeightsChanged();
    void viewsChanged();

private:
    explicit Settings(QObject *parent = nullptr);
    void write(const char *key, const QVariant &value);


    KSharedConfigPtr m_config;
    bool m_showExtensions = false;
    bool m_showHidden = false;
    bool m_detailsPane = false;
    QList<qreal> m_columnWeights;
};
