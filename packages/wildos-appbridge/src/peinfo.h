#pragma once

#include <QImage>
#include <QString>

// What can be read cheaply from a Windows program without running it: the
// names in its version information, its icon, whether it carries a
// signature, and what kind of file it is. Reads the PE format directly;
// nothing here executes the file.
struct PeInfo {
    enum Kind { NotWindows, Exe, Msi };

    Kind kind = NotWindows;
    bool is64Bit = false;
    bool dotNet = false;
    bool signedFile = false;
    QString productName;
    QString companyName;
    QString fileDescription;
    QString productVersion;
    QImage icon;

    static PeInfo read(const QString &path);
    // Reads only the icon, for launchers made from programs an installer
    // put in place.
    static QImage readIcon(const QString &path);
};
