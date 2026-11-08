#pragma once

#include <KJob>
#include <QList>
#include <QUrl>

#include <atomic>
#include <memory>

// Compress to ZIP file: the selection packed into one .zip next to it, as
// File Explorer does. The packing runs on a worker thread; the job reports
// bytes done through KIO's job tracker, the same progress a long copy in
// Files shows, and can be cancelled from there.
class ZipJob : public KJob
{
    Q_OBJECT

public:
    ZipJob(const QList<QUrl> &sources, const QUrl &target, QObject *parent = nullptr);

    void start() override;
    QUrl target() const { return m_target; }

protected:
    bool doKill() override;

private:
    QList<QUrl> m_sources;
    QUrl m_target;
    std::shared_ptr<std::atomic_bool> m_cancel = std::make_shared<std::atomic_bool>(false);
};
