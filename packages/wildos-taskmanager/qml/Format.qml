pragma Singleton

import QtQuick

// Numbers the way Windows Task Manager writes them: one decimal for CPU and
// rates, whole megabytes with thousands separators for memory.
QtObject {
    readonly property var locale: Qt.locale("en_GB")

    function cpu(value) {
        return Number(value || 0).toLocaleString(locale, "f", 1) + "%";
    }

    function percent(value) {
        return Math.round(value || 0) + "%";
    }

    function memory(bytes) {
        return Number(Math.round((bytes || 0) / 1048576)).toLocaleString(locale, "f", 0) + " MB";
    }

    function disk(bytesPerSecond) {
        const mb = (bytesPerSecond || 0) / 1048576;
        return (mb === 0 ? "0" : mb < 0.05 ? "0.1" : Number(mb).toLocaleString(locale, "f", 1)) + " MB/s";
    }

    function network(bytesPerSecond) {
        const mbps = (bytesPerSecond || 0) * 8 / 1000000;
        return (mbps === 0 ? "0" : mbps < 0.05 ? "0.1" : Number(mbps).toLocaleString(locale, "f", 1)) + " Mbps";
    }
}
