#include "settings.h"

#include <KConfigGroup>
#include <QDir>
#include <QSize>
#include <QStandardPaths>

namespace
{
const QList<qreal> designWeights = {2.4, 1.4, 1.1, 0.7};
}

Settings *Settings::instance()
{
    static Settings *settings = new Settings;
    return settings;
}

Settings *Settings::create(QQmlEngine *, QJSEngine *)
{
    auto settings = instance();
    QJSEngine::setObjectOwnership(settings, QJSEngine::CppOwnership);
    return settings;
}

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_config(KSharedConfig::openConfig(QStringLiteral("wildosfilesrc")))
{
    const KConfigGroup group(m_config, QStringLiteral("View"));
    m_showExtensions = group.readEntry("ShowExtensions", false);
    m_showHidden = group.readEntry("ShowHidden", false);
    m_detailsPane = group.readEntry("DetailsPane", false);
    m_columnWeights = group.readEntry("ColumnWeights", designWeights);
    if (m_columnWeights.size() != designWeights.size()) {
        m_columnWeights = designWeights;
    }
}

void Settings::write(const char *key, const QVariant &value)
{
    KConfigGroup group(m_config, QStringLiteral("View"));
    group.writeEntry(key, value);
    m_config->sync();
}

void Settings::setShowExtensions(bool on)
{
    if (on != m_showExtensions) {
        m_showExtensions = on;
        write("ShowExtensions", on);
        Q_EMIT showExtensionsChanged();
    }
}

void Settings::setShowHidden(bool on)
{
    if (on != m_showHidden) {
        m_showHidden = on;
        write("ShowHidden", on);
        Q_EMIT showHiddenChanged();
    }
}

void Settings::setDetailsPane(bool on)
{
    if (on != m_detailsPane) {
        m_detailsPane = on;
        write("DetailsPane", on);
        Q_EMIT detailsPaneChanged();
    }
}

void Settings::setColumnWeights(const QList<qreal> &weights)
{
    if (weights.size() == designWeights.size() && weights != m_columnWeights) {
        m_columnWeights = weights;
        KConfigGroup group(m_config, QStringLiteral("View"));
        group.writeEntry("ColumnWeights", weights);
        m_config->sync();
        Q_EMIT columnWeightsChanged();
    }
}

QString Settings::viewFor(const QUrl &url) const
{
    const KConfigGroup group(m_config, QStringLiteral("FolderViews"));
    // Photo and video folders open with large icons, as they do on
    // Windows; everything else starts in the design's medium icons.
    QString fallback = QStringLiteral("icons");
    // This computer lists its drives as tiles with a space bar, as on
    // Windows.
    if (url.scheme() == QLatin1String("wildos")) {
        fallback = QStringLiteral("tiles");
    }
    if (url.isLocalFile()) {
        const QString path = QDir::cleanPath(url.toLocalFile());
        for (auto location : {QStandardPaths::PicturesLocation, QStandardPaths::MoviesLocation}) {
            const QString media = QStandardPaths::writableLocation(location);
            if (!media.isEmpty() && media != QDir::homePath() && (path == media || path.startsWith(media + QLatin1Char('/')))) {
                fallback = QStringLiteral("large");
            }
        }
    }
    return group.readEntry(url.toString(QUrl::StripTrailingSlash), fallback);
}

QSize Settings::windowSize() const
{
    const KConfigGroup group(m_config, QStringLiteral("Window"));
    return group.readEntry("Size", QSize(980, 620));
}

void Settings::setWindowSize(const QSize &size)
{
    KConfigGroup group(m_config, QStringLiteral("Window"));
    group.writeEntry("Size", size);
    m_config->sync();
}

void Settings::setViewFor(const QUrl &url, const QString &mode)
{
    KConfigGroup group(m_config, QStringLiteral("FolderViews"));
    group.writeEntry(url.toString(QUrl::StripTrailingSlash), mode);
    m_config->sync();
    Q_EMIT viewsChanged();
}
