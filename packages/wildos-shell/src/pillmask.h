#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QRegion>
#include <QVariantList>

// Builds the region the panel's blur and popups follow from the glass pills
// the panel draws. QML has no way to make or combine a QRegion, and the
// panel view only accepts one region for the whole window.
class PillMask : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    Q_INVOKABLE QRegion region(const QVariantList &rects, qreal radius) const;
};
