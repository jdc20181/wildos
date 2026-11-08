#pragma once

#include <QIcon>
#include <QQuickImageProvider>
#include <QUrl>

// "image://theme/name" draws an icon from the current icon theme, so apps
// that Wild OS has its own icons for look the same here as in Start.
class ThemeIconProvider : public QQuickImageProvider
{
public:
    ThemeIconProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        const QString name = QUrl::fromPercentEncoding(id.toUtf8());
        const int edge = requestedSize.isValid() ? std::max(requestedSize.width(), requestedSize.height()) : 128;
        QIcon icon = QIcon::fromTheme(name);
        if (icon.isNull()) {
            icon = QIcon::fromTheme(QStringLiteral("application-x-executable"));
        }
        const QPixmap pixmap = icon.pixmap(QSize(edge, edge));
        if (size) {
            *size = pixmap.size();
        }
        return pixmap;
    }
};
