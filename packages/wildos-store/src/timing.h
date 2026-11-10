#pragma once

#include <QElapsedTimer>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcStore)

// Milliseconds since main() began. The timing lines are written at info
// level, so they only show with QT_LOGGING_RULES="wildos.store.info=true".
inline qint64 storeClock()
{
    static const QElapsedTimer clock = [] {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return clock.elapsed();
}
