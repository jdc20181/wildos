#pragma once

#include <QObject>

// `wildos-backup --watch`, the small user service behind catching up: when
// one of the person's backup drives is plugged in, it starts a catch-up
// run, which backs up only if a scheduled one was missed.
class Watcher : public QObject
{
    Q_OBJECT

public:
    explicit Watcher(QObject *parent = nullptr);
};
