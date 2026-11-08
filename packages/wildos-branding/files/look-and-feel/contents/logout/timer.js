.pragma library

// The greeter loads Logout.qml once per screen. As a shared library this
// file lets a key press on any screen stop the countdown on all of them.

var callbacks = [];

function addCancelAutoTriggerCallback(callback) {
    callbacks.push(callback);
}

function cancelAutoTrigger() {
    callbacks.forEach(function (callback) {
        if (callback) {
            callback();
        }
    });
}
