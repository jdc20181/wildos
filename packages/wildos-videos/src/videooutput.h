#pragma once

#include <QQuickFramebufferObject>
#include <QtQml/qqmlregistration.h>

// The picture: Player's mpv core draws each frame into this item's
// framebuffer through mpv's OpenGL render API, on Qt Quick's render
// thread. mpv allows one render context per core, so Videos has one of
// these, and picture in picture shrinks the window it is in instead of
// opening a second.
class VideoOutput : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit VideoOutput(QQuickItem *parent = nullptr);

    Renderer *createRenderer() const override;
};
