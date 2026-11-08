#pragma once

#include <QByteArray>
#include <QString>

// The Windows licence Windows mode activates with.
//
// PCs sold with Windows 8 or later carry their key in the ACPI MSDM table
// (Microsoft's "Software Licensing Tables (SLIC and MSDM)" specification):
// the standard 36 byte ACPI header, then the Software Licensing Structure
// of five 32 bit fields (Version 1, Reserved, Data Type 1, Data Reserved,
// Data Length 29) and the 29 character key. The table is readable by root
// only; winmode-firmware hands it over (see firmware.h).
namespace Licence
{
struct Msdm {
    bool valid = false;
    QString key;
    QString oemId;
    QString why;
};

Msdm parseMsdm(const QByteArray &table);

// Five groups of five from the product key alphabet (no A, E, I, L, O, S,
// U, Z, 0, 1 or 5), the shape Windows itself checks before asking
// Microsoft. Whether the key is genuine only Microsoft can say.
bool keyShapeValid(const QString &key);
// Upper case with the dashes put back, for a key typed loosely.
QString normaliseKey(const QString &typed);
QString keyEnd(const QString &key);

// "home", "pro" or "" from what a Windows installation says about itself:
// its EditionID ("Core", "Professional") or the original key's description
// ("[4.0] Core OEM:DM"). Education, Enterprise and Pro for Workstations
// have the Remote Desktop host too, so they count as pro here.
QString editionFrom(const QString &editionId, const QString &keyDescription = {});
// "Windows 11 Home", "Windows 11 Pro" or "Windows 11".
QString editionName(const QString &edition);
}
