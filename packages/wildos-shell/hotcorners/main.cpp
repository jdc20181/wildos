/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <main.h>
#include <plugin.h>

#include "hotcorners.h"

class KWIN_EXPORT HotCornersFactory : public KWin::PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    std::unique_ptr<KWin::Plugin> create() const override
    {
        if (KWin::kwinApp()->operationMode() != KWin::Application::OperationModeWayland) {
            return nullptr;
        }
        // The sign-in screen runs KWin too, with nothing for a corner to open.
        if (qEnvironmentVariable("XDG_SESSION_CLASS") == QLatin1String("greeter")) {
            return nullptr;
        }
        return std::make_unique<WildOS::HotCorners>();
    }
};

#include "main.moc"
