/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2026 Wild OS <dev@wildos.io>

    SPDX-License-Identifier: LGPL-2.0-or-later

    Plasma 6.3.6 task manager's PipeWireThumbnail.qml: KWin streams one
    window over PipeWire for as long as this item lives. Loaded through a
    Loader so the preview still opens, with the app's icon, on a system
    without the PipeWire QML module.
*/

import QtQuick
import org.kde.pipewire as PipeWire
import org.kde.taskmanager as TaskManager

PipeWire.PipeWireSourceItem {
    id: stream

    property var winId

    readonly property bool hasThumbnail: ready && streamSize.width > 0 && streamSize.height > 0

    nodeId: request.nodeId

    TaskManager.ScreencastingRequest {
        id: request
        uuid: String(stream.winId ?? "")
    }
}
