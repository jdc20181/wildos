import QtQuick
import org.kde.bluezqt as BluezQt
import org.kde.plasma.private.bluetooth as PlasmaBt

// Bluetooth through BluezQt and bluedevil's models, as the stock Bluetooth
// applet does. A machine with no adapter reports unavailable.
Item {
    id: bluetooth

    readonly property bool available: BluezQt.Manager.adapters.length > 0 || BluezQt.Manager.bluetoothBlocked
    readonly property bool enabled: BluezQt.Manager.bluetoothOperational
    readonly property var connectedDevices: BluezQt.Manager.connectedDevices
    readonly property var devices: devicesModel
    readonly property bool discovering: BluezQt.Manager.adapters.some(adapter => adapter.discovering)

    PlasmaBt.DevicesProxyModel {
        id: devicesModel
        hideBlockedDevices: true
        // Set once rather than bound: the shared model re-announces itself,
        // which a binding reports as a loop.
        Component.onCompleted: sourceModel = PlasmaBt.SharedDevicesStateProxyModel
    }

    function setEnabled(on: bool): void {
        BluezQt.Manager.bluetoothBlocked = !on;
        BluezQt.Manager.adapters.forEach(adapter => {
            adapter.powered = on;
        });
    }

    // A device found while looking is paired first, then connected once
    // pairing goes through.
    function toggleDevice(device: var, ubi: string, connected: bool, paired: bool): void {
        if (!paired) {
            const pairing = device.pair();
            pairing.finished.connect(() => {
                if (!pairing.error) {
                    device.trusted = true;
                    PlasmaBt.SharedDevicesStateProxyModel.registerPendingCallForDeviceUbi(device.connectToDevice(), ubi);
                }
            });
            PlasmaBt.SharedDevicesStateProxyModel.registerPendingCallForDeviceUbi(pairing, ubi);
            return;
        }
        const call = connected ? device.disconnectFromDevice() : device.connectToDevice();
        PlasmaBt.SharedDevicesStateProxyModel.registerPendingCallForDeviceUbi(call, ubi);
    }

    // Windows' "Add a device": look for devices in pairing mode for half a
    // minute, listing them with the known ones.
    function addDevice(): void {
        BluezQt.Manager.adapters.forEach(adapter => {
            if (adapter.powered && !adapter.discovering) {
                adapter.startDiscovery();
            }
        });
        stopLooking.restart();
    }

    Timer {
        id: stopLooking
        interval: 30000
        onTriggered: BluezQt.Manager.adapters.forEach(adapter => {
            if (adapter.discovering) {
                adapter.stopDiscovery();
            }
        })
    }
}
