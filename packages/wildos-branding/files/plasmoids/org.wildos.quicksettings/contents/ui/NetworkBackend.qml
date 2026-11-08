import QtQuick
import org.kde.kitemmodels as KItemModels
import org.kde.plasma.networkmanagement as PlasmaNM
import org.kde.networkmanager as NMQt

// Wi-Fi, flight mode and VPN through plasma-nm, the same NetworkManager
// bindings the stock Networks applet uses. Loaded on its own so a machine
// without plasma-nm still gets the rest of the flyout.
Item {
    id: network

    readonly property bool wifiAvailable: availableDevices.wirelessDeviceAvailable
    readonly property bool wifiEnabled: enabledConnections.wirelessEnabled
    readonly property bool flightAvailable: availableDevices.wirelessDeviceAvailable
        || availableDevices.modemDeviceAvailable
    readonly property bool flightMode: PlasmaNM.Configuration.airplaneModeEnabled
    readonly property string iconName: connectionIcon.connectionIcon
    readonly property bool connecting: connectionIcon.connecting
    readonly property bool wiredAvailable: availableDevices.wiredDeviceAvailable
    readonly property bool wiredConnected: activeWired.count > 0
    readonly property bool wifiConnected: activeWifi.count > 0
    readonly property bool portal: networkStatus.connectivity === NMQt.NetworkManager.Portal
    // NetworkManager only knows about the internet when its connectivity
    // check is set up; without one this stays unknown.
    readonly property bool internetKnown: networkStatus.connectivity !== NMQt.NetworkManager.UnknownConnectivity
    readonly property bool internet: networkStatus.connectivity === NMQt.NetworkManager.Full
    readonly property string networkCheckUrl: networkStatus.networkCheckUrl
    readonly property bool scanning: handler.scanning

    readonly property var wifiModel: wifiConnections
    readonly property var vpnModel: vpnConnections
    readonly property bool vpnActive: activeVpns.count > 0
    readonly property bool vpnConfigured: vpnConnections.count > 0
    property int revision: 0
    readonly property string activeVpnName: revision >= 0 && vpnActive ? rowValue(activeVpns, 0, "Name") : ""
    readonly property string activeWifiName: revision >= 0 && activeWifi.count > 0 ? rowValue(activeWifi, 0, "ItemUniqueName") : ""

    signal activationFailed(string message)

    PlasmaNM.Handler {
        id: handler
        onConnectionActivationFailed: (path, message) => network.activationFailed(message)
    }
    PlasmaNM.EnabledConnections { id: enabledConnections }
    PlasmaNM.AvailableDevices { id: availableDevices }
    PlasmaNM.NetworkStatus { id: networkStatus }
    PlasmaNM.ConnectionIcon {
        id: connectionIcon
        connectivity: networkStatus.connectivity
    }

    PlasmaNM.NetworkModel { id: networkModel }

    // The stock applet's proxy hides duplicates and connections that cannot
    // be shown, and sorts active connections first.
    PlasmaNM.AppletProxyModel {
        id: appletModel
        sourceModel: networkModel
    }

    KItemModels.KSortFilterProxyModel {
        id: wifiConnections
        sourceModel: appletModel
        filterRowCallback: (row, parent) => {
            const type = network.cell(sourceModel, row, parent, "Type");
            return type === PlasmaNM.Enums.Wireless;
        }
    }

    KItemModels.KSortFilterProxyModel {
        id: activeWifi
        sourceModel: wifiConnections
        filterRowCallback: (row, parent) => {
            const state = network.cell(sourceModel, row, parent, "ConnectionState");
            return state === PlasmaNM.Enums.Activated;
        }
    }

    KItemModels.KSortFilterProxyModel {
        id: activeWired
        sourceModel: appletModel
        filterRowCallback: (row, parent) => network.cell(sourceModel, row, parent, "Type") === PlasmaNM.Enums.Wired
            && network.cell(sourceModel, row, parent, "ConnectionState") === PlasmaNM.Enums.Activated
    }

    KItemModels.KSortFilterProxyModel {
        id: vpnConnections
        sourceModel: appletModel
        filterRowCallback: (row, parent) => {
            const type = network.cell(sourceModel, row, parent, "Type");
            return type === PlasmaNM.Enums.Vpn;
        }
    }

    KItemModels.KSortFilterProxyModel {
        id: activeVpns
        sourceModel: vpnConnections
        filterRowCallback: (row, parent) => {
            const state = network.cell(sourceModel, row, parent, "ConnectionState");
            return state === PlasmaNM.Enums.Activated || state === PlasmaNM.Enums.Activating;
        }
    }

    // The most recently used VPN comes first, so the tile reconnects to it.
    KItemModels.KSortFilterProxyModel {
        id: vpnsByLastUse
        sourceModel: vpnConnections
        sortRoleName: "TimeStamp"
        sortOrder: Qt.DescendingOrder
    }

    // KSortFilterProxyModel caches its filter, so state changes in the source
    // have to ask it to filter again.
    Connections {
        target: networkModel
        function onDataChanged() {
            activeWifi.invalidateFilter();
            activeWired.invalidateFilter();
            activeVpns.invalidateFilter();
            network.revision++;
        }
    }

    function cell(model, row, parent, roleName): var {
        return model.data(model.index(row, 0, parent), model.KItemModels.KRoleNames.role(roleName));
    }

    function rowValue(model, row, roleName): var {
        return model.data(model.index(row, 0), model.KItemModels.KRoleNames.role(roleName));
    }

    function setWifiEnabled(on: bool): void {
        handler.enableWireless(on);
    }

    function setFlightMode(on: bool): void {
        handler.enableAirplaneMode(on);
        PlasmaNM.Configuration.airplaneModeEnabled = on;
    }

    function requestScan(): void {
        if (wifiEnabled && !flightMode) {
            handler.requestScan();
        }
    }

    function activate(connectionPath: string, devicePath: string, specificPath: string): void {
        handler.activateConnection(connectionPath, devicePath, specificPath);
    }

    function addAndActivate(devicePath: string, specificPath: string, password: string): void {
        if (password.length > 0) {
            handler.addAndActivateConnection(devicePath, specificPath, password);
        } else {
            handler.addAndActivateConnection(devicePath, specificPath);
        }
    }

    function deactivate(connectionPath: string, devicePath: string): void {
        handler.deactivateConnection(connectionPath, devicePath);
    }

    function forget(connectionPath: string): void {
        handler.removeConnection(connectionPath);
    }

    // Returns false when there is no VPN to switch on, so the caller can show
    // the VPN page instead.
    function toggleVpn(): bool {
        if (vpnActive) {
            for (let i = 0; i < activeVpns.count; i++) {
                deactivate(rowValue(activeVpns, i, "ConnectionPath"), rowValue(activeVpns, i, "DevicePath"));
            }
            return true;
        }
        if (vpnsByLastUse.count === 0) {
            return false;
        }
        activate(rowValue(vpnsByLastUse, 0, "ConnectionPath"), rowValue(vpnsByLastUse, 0, "DevicePath"),
            rowValue(vpnsByLastUse, 0, "SpecificPath"));
        return true;
    }

    function setHoldUpdates(row: int, hold: bool): void {
        const source = wifiConnections.mapToSource(wifiConnections.index(row, 0));
        appletModel.setData(source, hold, PlasmaNM.NetworkModel.DelayModelUpdatesRole);
    }
}
