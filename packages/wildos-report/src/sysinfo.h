#pragma once

#include "redactor.h"

#include <QString>

// What a report says about this PC, and what the redactor must take out.
namespace SysInfo
{
// "1.2.1", or empty off Wild OS.
QString version();
// "stable" or "beta": the Suites line of the Wild OS source.
QString channel();
// "Dell Inc. XPS 13 9310", or empty when the firmware doesn't say.
QString model();
// Wild OS version, model, processor, graphics, memory, firmware, session
// and screens, one per line, plus the version of `package` if given.
// `screens` lists the displays and needs a GUI application.
QString aboutPc(const QString &package, bool screens);

// The people on this PC and its names and numbers, read without any
// privilege: accounts, host name, saved Wi-Fi networks (NetworkManager),
// disk and USB serial numbers, the machine and boot IDs. `extraSerials`
// are the ones only root can read, from report-logs.
Redactor::Known known(const QStringList &extraSerials);
}
