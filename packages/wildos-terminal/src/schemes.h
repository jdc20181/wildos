#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QVariantList>

// The colour schemes Terminal ships, read from the same Konsole format
// files the terminal widget draws with, so the previews in Settings show
// exactly the colours a tab will use.
struct Scheme {
    QString id;
    QString name;
    QString path;
    QColor background;
    QColor foreground;
    QColor foregroundBold;
    QList<QColor> colours; // 0-7 normal, 8-15 bright
};

namespace Schemes
{
QString directory();
const QList<Scheme> &all();
const Scheme &find(const QString &id);
QVariantList toVariant();
}
