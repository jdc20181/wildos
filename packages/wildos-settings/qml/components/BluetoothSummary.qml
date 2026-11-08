import QtQuick
import org.kde.bluezqt as BluezQt

// Bluetooth state for Home: whether there is an adapter, whether it is on,
// and how many devices are connected. Switching it uses rfkill the way the
// tray icon does.
QtObject {
    id: summary

    readonly property var manager: BluezQt.Manager
    readonly property bool present: manager.adapters.length > 0
    readonly property bool on: manager.bluetoothOperational
    property int connectedCount: 0

    function recount() {
        let n = 0;
        for (const device of manager.devices) {
            if (device.connected) {
                ++n;
            }
        }
        connectedCount = n;
    }

    function setOn(enabled) {
        manager.bluetoothBlocked = !enabled;
        for (const adapter of manager.adapters) {
            adapter.powered = enabled;
        }
    }

    readonly property string caption: !present ? "No Bluetooth on this computer"
        : !on ? "Off"
        : connectedCount === 0 ? "On, nothing connected"
        : "On, " + connectedCount + (connectedCount === 1 ? " device connected" : " devices connected")

    property Connections watch: Connections {
        target: summary.manager
        function onDeviceChanged() { summary.recount(); }
        function onDeviceAdded() { summary.recount(); }
        function onDeviceRemoved() { summary.recount(); }
    }

    Component.onCompleted: recount()
}
