/*
    SPDX-FileCopyrightText: 2026 Wild OS Developers <dev@wildos.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <main.h>
#include <plugin.h>

#include "windowmenu.h"

class KWIN_EXPORT WindowMenuFactory : public KWin::PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    std::unique_ptr<KWin::Plugin> create() const override
    {
        // Wild OS runs on Wayland only; on X11 KWin keeps its own menu.
        if (KWin::kwinApp()->operationMode() != KWin::Application::OperationModeWayland) {
            return nullptr;
        }
        return std::make_unique<WildOS::WindowMenu>();
    }
};

#include "main.moc"
