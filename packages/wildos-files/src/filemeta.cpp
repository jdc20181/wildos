#include "filemeta.h"

#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/PropertyInfo>
#include <KFileMetaData/SimpleExtractionResult>
#include <QCoreApplication>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QPointer>
#include <QtConcurrentRun>

namespace
{
// Extractors name things in title case ("Image Width"); Wild OS writes
// sentence case. Words in capitals (ISO, GPS) stay as they are.
QString sentenceCase(const QString &label)
{
    QStringList words = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (int i = 1; i < words.size(); ++i) {
        if (words[i] != words[i].toUpper()) {
            words[i] = words[i].toLower();
        }
    }
    return words.join(QLatin1Char(' '));
}
}

FileMeta::FileMeta(QObject *parent)
    : QObject(parent)
{
}

void FileMeta::setUrl(const QUrl &url)
{
    if (url == m_url) {
        return;
    }
    m_url = url;
    Q_EMIT urlChanged();
    m_fields.clear();
    const int generation = ++m_generation;
    if (!url.isLocalFile() || QFileInfo(url.toLocalFile()).isDir()) {
        m_loading = false;
        Q_EMIT fieldsChanged();
        return;
    }
    m_loading = true;
    Q_EMIT fieldsChanged();

    const QString path = url.toLocalFile();
    QPointer<FileMeta> self(this);
    (void)QtConcurrent::run([path, self, generation] {
        const QString mime = QMimeDatabase().mimeTypeForFile(path).name();
        KFileMetaData::ExtractorCollection collection;
        KFileMetaData::SimpleExtractionResult result(path, mime, KFileMetaData::ExtractionResult::ExtractMetaData);
        const auto extractors = collection.fetchExtractors(mime);
        for (KFileMetaData::Extractor *extractor : extractors) {
            extractor->extract(&result);
        }
        QVariantList fields;
        const auto properties = result.properties();
        for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
            const KFileMetaData::PropertyInfo info(it.key());
            const QString value = info.formatAsDisplayString(it.value()).trimmed();
            if (!info.displayName().isEmpty() && !value.isEmpty()) {
                fields.append(QVariantMap{{QStringLiteral("label"), sentenceCase(info.displayName())}, {QStringLiteral("value"), value}});
            }
        }
        QMetaObject::invokeMethod(qApp, [self, generation, fields] {
            if (!self || self->m_generation != generation) {
                return;
            }
            self->m_fields = fields;
            self->m_loading = false;
            Q_EMIT self->fieldsChanged();
        });
    });
}
