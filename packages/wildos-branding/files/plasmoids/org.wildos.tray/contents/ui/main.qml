pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kitemmodels as KItemModels
import org.kde.plasma.private.systemtray as SystemTray
import org.kde.plasma.private.devicenotifier as DeviceNotifier
import org.wildos.shell as WildOS

// The Windows 11 style corner of the taskbar: the arrow that opens the hidden
// app icons, a few running apps' icons inline, and a "safely remove" icon
// while a USB drive is in use. App icons are the status icons apps publish
// (StatusNotifierItems), read through Plasma's own model. A cup shows while
// a hot corner keeps the computer awake. Plasma's widgets
// (display settings, disks and the like) are never shown here.
PlasmoidItem {
    id: tray

    // What the popup shows: "overflow" for the hidden icons, "usb" for the
    // drives that can be ejected.
    property string mode: "overflow"

    readonly property var hiddenItems: Plasmoid.configuration.hiddenItems
    readonly property int maxInline: Plasmoid.configuration.maxInline

    SystemTray.StatusNotifierModel {
        id: allItems
    }

    // Bumped whenever an icon's data changes, so the inline and hidden
    // split is worked out again.
    property int revision: 0

    Connections {
        target: allItems
        function onDataChanged() { tray.revision++; }
        function onRowsInserted() { tray.revision++; }
        function onRowsRemoved() { tray.revision++; }
        function onModelReset() { tray.revision++; }
    }

    function value(model: var, row: int, parent: var, role: string): var {
        return model.data(model.index(row, 0, parent), model.KItemModels.KRoleNames.role(role));
    }

    // Desktop plumbing, and apps that are not ready to draw yet, stay out.
    readonly property KItemModels.KSortFilterProxyModel items: KItemModels.KSortFilterProxyModel {
        sourceModel: allItems
        filterRowCallback: (row, parent) => {
            if (tray.value(allItems, row, parent, "canRender") === false) {
                return false;
            }
            const id = String(tray.value(allItems, row, parent, "itemId") || "");
            const title = String(tray.value(allItems, row, parent, "Title") || "");
            if (id.length === 0 && title.length === 0) {
                return false;
            }
            if (tray.value(allItems, row, parent, "status") === PlasmaCore.Types.HiddenStatus) {
                return false;
            }
            return !tray.hiddenItems.some(hidden => hidden.length > 0 && (id === hidden || title === hidden));
        }
    }

    onRevisionChanged: items.invalidateFilter()
    onHiddenItemsChanged: items.invalidateFilter()

    // Filtering again can move rows about, so the split below is redone
    // whenever the filtered list changes shape.
    property int layoutRevision: 0

    Connections {
        target: tray.items
        function onRowsInserted() { tray.layoutRevision++; }
        function onRowsRemoved() { tray.layoutRevision++; }
        function onLayoutChanged() { tray.layoutRevision++; }
        function onModelReset() { tray.layoutRevision++; }
        function onDataChanged() { tray.layoutRevision++; }
    }

    // Active apps sit on the taskbar, up to maxInline of them; apps asking
    // for attention always do. Everything else waits behind the arrow. The
    // app's own status is used, not Plasma's tray settings, which this
    // widget does not have.
    readonly property var inlineRows: {
        if (revision < 0 || layoutRevision < 0) {
            return [];
        }
        const rows = [];
        for (let i = 0; i < items.count; i++) {
            const status = value(items, i, undefined, "status");
            if (status === PlasmaCore.Types.NeedsAttentionStatus) {
                rows.push(i);
            } else if (status === PlasmaCore.Types.ActiveStatus && rows.length < maxInline) {
                rows.push(i);
            }
        }
        return rows;
    }
    readonly property int overflowCount: items.count - inlineRows.length

    // Removable drives that are mounted, for the "safely remove" icon.
    DeviceNotifier.DeviceFilterControl {
        id: removables
        filterType: DeviceNotifier.DeviceFilterControl.Removable
    }

    readonly property KItemModels.KSortFilterProxyModel mountedDrives: KItemModels.KSortFilterProxyModel {
        sourceModel: removables
        filterRowCallback: (row, parent) => tray.value(removables, row, parent, "deviceMounted") === true
    }

    Connections {
        target: removables
        function onDataChanged() { tray.mountedDrives.invalidateFilter(); }
    }

    function open(newMode: string): void {
        if (expanded && mode === newMode) {
            expanded = false;
            return;
        }
        mode = newMode;
        expanded = true;
    }

    onOverflowCountChanged: {
        if (overflowCount === 0 && mode === "overflow") {
            expanded = false;
        }
    }

    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    Plasmoid.title: "App icons"
    // Hidden, not Passive, when there is nothing to show: the panel then
    // leaves the empty tray out, so it adds no gap between the divider and
    // Wi-Fi, volume and battery.
    Plasmoid.status: items.count > 0 || mountedDrives.count > 0 || WildOS.KeepAwake.active ? PlasmaCore.Types.ActiveStatus : PlasmaCore.Types.HiddenStatus
    toolTipMainText: ""
    toolTipSubText: ""

    preferredRepresentation: compactRepresentation
    compactRepresentation: TrayRow {
        tray: tray
    }
    fullRepresentation: TrayPopup {
        tray: tray
    }
}
