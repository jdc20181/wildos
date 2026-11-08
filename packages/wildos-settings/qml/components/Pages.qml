pragma Singleton

import QtQuick
import WildOS.Settings

// Every page Settings can show, under the names Windows 11 uses. A page is
// either a Wild OS page ("component"), a KDE module shown inside the window
// ("kcm") or a KDE module that is still built from widgets and opens in a
// window of its own ("window"). KDE-only modules such as Plasma Search,
// User Feedback or Desktop Effects are deliberately absent, so neither the
// navigation nor search can reach them.
QtObject {
    id: pages

    readonly property var navigation: ["home", "system", "devices", "network", "personalisation", "apps",
        "windowsapps", "accounts", "time", "privacy", "update"]

    readonly property var all: ({
        "home": { title: "Home", icon: "home", component: "HomePage",
            keywords: "start overview recommended" },

        "system": { title: "System", icon: "system", component: "CategoryPage",
            children: ["display", "sound", "notifications", "power", "storage", "multitasking", "about"],
            keywords: "system" },
        "display": { title: "Display", parent: "system", icon: "display", component: "DisplayPage", fallback: "kcm_kscreen",
            description: "Monitors, brightness, resolution and scale",
            keywords: "screen monitor resolution scale scaling zoom brightness refresh rate hz orientation rotate hdr second screen projector" },
        "nightlight": { title: "Night light", parent: "display", icon: "moon", component: "NightLightPage", fallback: "kcm_nightlight",
            description: "Warmer colours in the evening",
            keywords: "blue light warm colour color temperature evening sleep" },
        "sound": { title: "Sound", parent: "system", icon: "sound", component: "SoundPage", fallback: "kcm_pulseaudio",
            description: "Volume, output and input devices",
            keywords: "volume audio speakers headphones headset microphone mic input output mute balance" },
        "notifications": { title: "Notifications", parent: "system", icon: "bell", component: "NotificationsPage", fallback: "kcm_notifications",
            description: "Alerts from apps and Wild OS, do not disturb",
            keywords: "alerts do not disturb focus banners popups badges" },
        "power": { title: "Power and battery", parent: "system", icon: "battery", component: "PowerPage", fallback: "kcm_powerdevilprofilesconfig",
            description: "Sleep, screen off and battery saving",
            keywords: "battery sleep suspend hibernate lid screen off energy power mode saver" },
        "storage": { title: "Storage", parent: "system", icon: "storage", component: "StoragePage",
            description: "Disk space and the Recycle Bin",
            keywords: "disk space drive free space bin trash recycle hard drive ssd clean up" },
        "multitasking": { title: "Multitasking", parent: "system", icon: "multitasking", component: "MultitaskingPage",
            description: "Snap windows, desktops and Alt+Tab",
            keywords: "snap layouts alt tab task switcher windows side by side" },
        "displayadvanced": { title: "Advanced display", parent: "display", icon: "display", kcm: "kcm_kscreen",
            description: "Arrange screens, refresh rate, HDR and more",
            keywords: "arrange screens multiple monitors refresh rate hdr overscan rgb range" },
        "soundadvanced": { title: "All sound devices", parent: "sound", icon: "sound", kcm: "kcm_pulseaudio",
            description: "Every device and app volume, profiles",
            keywords: "app volume mixer devices profiles" },
        "appnotifications": { title: "Notifications from apps", parent: "notifications", icon: "bell", kcm: "kcm_notifications",
            description: "Choose which apps can notify you",
            keywords: "app notifications sounds popups per app" },
        "poweradvanced": { title: "More power settings", parent: "power", icon: "battery", kcm: "kcm_powerdevilprofilesconfig",
            description: "Lid, power button and dimming",
            keywords: "lid close power button dim screen brightness battery critical" },
        "desktops": { title: "Desktops", parent: "multitasking", icon: "desktops", kcm: "kcm_kwin_virtualdesktops",
            description: "Extra desktops for separate sets of windows",
            keywords: "virtual desktops workspaces" },
        "about": { title: "About", parent: "system", icon: "info", component: "AboutPage",
            description: "Device specifications, rename this PC",
            keywords: "device name rename pc computer name hostname specifications specs processor cpu memory ram version secure boot" },

        "devices": { title: "Bluetooth and devices", icon: "bluetooth", component: "CategoryPage",
            children: ["bluetooth", "mouse", "touchpad", "keyboard", "printers"],
            keywords: "devices peripherals" },
        "bluetooth": { title: "Bluetooth", parent: "devices", icon: "bluetooth", component: "BluetoothPage", fallback: "kcm_bluetooth",
            description: "Pair headphones, speakers and other devices",
            keywords: "pair pairing headphones speakers earbuds bluetooth devices connect controller" },
        "mouse": { title: "Mouse", parent: "devices", icon: "mouse", component: "MousePage", fallback: "kcm_mouse",
            description: "Pointer speed, buttons and scrolling",
            keywords: "pointer cursor speed scroll buttons left handed acceleration" },
        "touchpad": { title: "Touchpad", parent: "devices", icon: "touchpad", component: "TouchpadPage", fallback: "kcm_touchpad",
            description: "Taps, scrolling and gestures",
            keywords: "trackpad tap to click gestures scrolling laptop" },
        "keyboard": { title: "Keyboard", parent: "devices", icon: "keyboard", component: "KeyboardPage", fallback: "kcm_keyboard",
            description: "Layouts, key repeat and Num Lock",
            keywords: "keyboard layout layouts language typing input repeat delay rate num lock caps lock switch" },
        "keyboardadvanced": { title: "More keyboard options", parent: "keyboard", icon: "keyboard", kcm: "kcm_keyboard",
            description: "Keyboard model, compose key and other special keys",
            keywords: "compose key keyboard model alt gr special characters xkb options" },
        "printers": { title: "Printers and scanners", parent: "devices", icon: "printer", component: "PrintersPage", fallback: "kcm_printer_manager",
            description: "Add and manage printers",
            keywords: "printer printers print scanner add printer default printer test page" },
        "printersadvanced": { title: "More printer settings", parent: "printers", icon: "printer", kcm: "kcm_printer_manager",
            description: "Printers that need a driver, USB printers and print queues",
            keywords: "printer driver usb printer print queue share printer" },

        "network": { title: "Network and internet", icon: "wifi", component: "NetworkPage",
            keywords: "wifi wi-fi wireless internet ethernet network airplane flight mode connect hotspot" },
        "networkadvanced": { title: "Advanced network settings", parent: "network", icon: "globe", window: "kcm_networkmanagement",
            description: "VPN, proxy, IP addresses and saved networks",
            keywords: "vpn proxy dns ip address advanced connections saved networks forget" },

        "personalisation": { title: "Personalisation", icon: "palette", component: "PersonalisationPage",
            children: ["background", "colours", "lockscreen", "taskbar"],
            keywords: "personalize personalise customise customize theme look light dark font fonts text size bigger" },
        "background": { title: "Background", parent: "personalisation", icon: "image", component: "BackgroundPage",
            description: "Pictures and your own photos",
            keywords: "wallpaper background desktop picture photo image" },
        "colours": { title: "Colours", parent: "personalisation", icon: "drop", component: "ColoursPage",
            description: "Accent colour",
            keywords: "accent colour color colors highlight theme" },
        "lockscreen": { title: "Lock screen", parent: "personalisation", icon: "lock", component: "LockScreenPage",
            description: "Lock screen picture and screen lock",
            keywords: "lock screen picture wallpaper" },
        "taskbar": { title: "Taskbar", parent: "personalisation", icon: "taskbar", component: "TaskbarPage",
            description: "Hiding the taskbar",
            keywords: "taskbar panel auto hide autohide" },

        "apps": { title: "Apps", icon: "apps", component: "CategoryPage",
            children: ["installedapps", "defaultapps", "startup"],
            keywords: "apps programs applications" },
        "installedapps": { title: "Installed apps", parent: "apps", icon: "list", component: "InstalledAppsPage",
            description: "Uninstall apps",
            keywords: "uninstall remove programs apps installed software add or remove" },
        "defaultapps": { title: "Default apps", parent: "apps", icon: "default-apps", component: "DefaultAppsPage", fallback: "kcm_componentchooser",
            description: "Browser, email and other default apps",
            keywords: "default browser email mail open with file associations" },
        "startup": { title: "Startup", parent: "apps", icon: "startup", component: "StartupPage", fallback: "kcm_autostart",
            description: "Apps that start when you sign in",
            keywords: "startup autostart login sign in boot" },

        "windowsapps": { title: "Windows apps", icon: "windowsapps", component: "WindowsAppsPage",
            keywords: "windows exe programs sandbox" },
        "windowsmode": { title: "Windows mode", parent: "windowsapps", icon: "windowsapps", component: "WindowsModePage",
            description: "Real Windows in the background, for Office and other programs",
            keywords: "windows mode virtual machine vm office word excel licence product key activation remoteapp" },

        "accounts": { title: "Accounts", icon: "account", component: "AccountsPage",
            keywords: "account user profile" },
        "users": { title: "Your info", parent: "accounts", icon: "account", component: "YourInfoPage", fallback: "kcm_users",
            description: "Your name and picture",
            keywords: "user picture avatar photo name profile account picture" },
        "signin": { title: "Sign-in options", parent: "accounts", icon: "lock", component: "SignInPage",
            description: "Password and automatic sign-in",
            keywords: "password change password hint automatic sign in auto login autologin" },
        "otherusers": { title: "Other users", parent: "accounts", icon: "people", component: "OtherUsersPage",
            description: "Add and remove the people who share this computer",
            keywords: "add user other users family administrator standard user remove account delete account" },

        "time": { title: "Time and language", icon: "clock", component: "TimePage",
            keywords: "time date clock language" },
        "datetime": { title: "Date and time", parent: "time", icon: "clock", component: "DateTimePage", fallback: "kcm_clock",
            description: "Change the date, time and time zone",
            keywords: "date time zone timezone clock" },
        "language": { title: "Language and region", parent: "time", icon: "language", component: "LanguagePage", fallback: "kcm_regionandlang",
            description: "Display language, formats and region",
            keywords: "language display language region format formats regional format currency locale country" },
        "formatsadvanced": { title: "More format options", parent: "language", icon: "language", kcm: "kcm_regionandlang",
            description: "Numbers, time, money and measurements separately",
            keywords: "number format currency format measurement units paper size" },

        "privacy": { title: "Privacy and security", icon: "shield", component: "PrivacyPage",
            keywords: "security privacy protection encryption firewall secure boot sandbox" },
        "apppermissions": { title: "App permissions", parent: "privacy", icon: "sandbox", component: "AppPermissionsPage", fallback: "kcm_flatpak",
            description: "What apps from the Store can reach",
            keywords: "permissions sandbox app access" },
        "camera": { title: "Camera", parent: "apppermissions", icon: "camera", component: "AppAccessPage",
            description: "Apps that can use your camera",
            keywords: "camera webcam video call" },
        "microphone": { title: "Microphone", parent: "apppermissions", icon: "mic", component: "AppAccessPage",
            description: "Apps that can use your microphone",
            keywords: "microphone mic recording voice" },
        "location": { title: "Location", parent: "apppermissions", icon: "location", component: "AppAccessPage",
            description: "Apps that can find where you are",
            keywords: "location gps where position" },
        "appfiles": { title: "Files and folders", parent: "apppermissions", icon: "folder", component: "AppAccessPage",
            description: "Apps that can open your folders",
            keywords: "files folders documents pictures file access" },
        "appnetwork": { title: "Internet", parent: "apppermissions", icon: "globe", component: "AppAccessPage",
            description: "Apps that can go online",
            keywords: "internet network online offline" },
        "apppermissionsadvanced": { title: "Everything an app can reach", parent: "apppermissions", icon: "sandbox", kcm: "kcm_flatpak",
            description: "Every sandbox permission, app by app",
            keywords: "flatpak permissions dbus environment devices advanced" },
        "screenlock": { title: "Screen lock", parent: "privacy", icon: "lock", component: "ScreenLockPage", fallback: "kcm_screenlocker",
            description: "Lock after a while and ask for your password",
            keywords: "lock screen timeout password sign in wake" },

        "update": { title: "Updates", icon: "update", component: "UpdatePage",
            keywords: "update updates upgrade patch security updates check for updates install" }
    })

    function page(id) {
        return all[id] || null;
    }

    // Other names callers use for a page: Windows' own spellings, the ids
    // Start's search used before it knew these, and KDE module names, so
    // "wildos-settings --page kcm_bluetooth" lands on Bluetooth.
    readonly property var aliases: ({
        "personalization": "personalisation",
        "colors": "colours",
        "wallpaper": "background",
        "lock-screen": "lockscreen",
        "time-language": "time",
        "windows-apps": "windowsapps",
        "updates": "update",
        "security": "privacy",
        "network-advanced": "networkadvanced",
        "kcm_kscreen": "display",
        "kcm_networkmanagement": "networkadvanced",
        "kcm_wallpaper": "background",
        "kcm_lookandfeel": "personalisation",
        "kcm_colors": "colours",
        "kcm_about-distro": "about",
        "kcm_updates": "update",
        "kcm_cursortheme": "mouse",
        "kcm_proxy": "networkadvanced",
        "kcm_soundtheme": "sound",
        "kcm_energyinfo": "power",
        "kcm_virtualkeyboard": "keyboard",
        // Text size in Personalisation is the Wild OS way to change fonts.
        "kcm_fonts": "personalisation",
        "fonts": "personalisation"
    })

    function resolve(id) {
        if (!id) {
            return "home";
        }
        if (all[id]) {
            return id;
        }
        if (aliases[id]) {
            return aliases[id];
        }
        // A module Wild OS has its own page for opens that page.
        for (const key in all) {
            if (all[key].fallback === id) {
                return key;
            }
        }
        for (const key in all) {
            if (all[key].kcm === id || all[key].window === id) {
                return key;
            }
        }
        return id;
    }

    function exists(id) {
        const p = all[id];
        if (!p) {
            return false;
        }
        if (p.kcm) {
            return KcmHost.available(p.kcm);
        }
        if (p.window) {
            return KcmHost.available(p.window);
        }
        return true;
    }

    // The navigation entry a page lives under, for highlighting.
    function category(id) {
        let p = all[id];
        let current = id;
        while (p && p.parent) {
            current = p.parent;
            p = all[current];
        }
        return current;
    }

    // Parents first, ending with the page itself.
    function trail(id) {
        const chain = [];
        let current = id;
        while (current && all[current]) {
            chain.unshift(current);
            current = all[current].parent;
        }
        return chain;
    }

    function children(id) {
        const p = all[id];
        return p && p.children ? p.children.filter(exists) : [];
    }

    function normalise(text) {
        return text.toLowerCase().replace(/[-_]/g, "").replace(/\s+/g, " ").trim();
    }

    function search(text) {
        const query = normalise(text);
        if (query.length === 0) {
            return [];
        }
        const results = [];
        for (const id in all) {
            if (!exists(id)) {
                continue;
            }
            const p = all[id];
            const title = normalise(p.title);
            const words = normalise((p.keywords || "") + " " + (p.description || "")).split(" ");
            let score = 0;
            if (title.startsWith(query)) {
                score = 4;
            } else if (title.includes(query)) {
                score = 3;
            } else if (words.some(w => w === query)) {
                score = 2;
            } else if (query.length >= 3 && words.some(w => w.startsWith(query))) {
                score = 1;
            } else if (query.includes(" ") && normalise(p.keywords || "").includes(query)) {
                score = 1;
            }
            if (score > 0) {
                results.push({ id: id, score: score });
            }
        }
        results.sort((a, b) => b.score - a.score || all[a.id].title.localeCompare(all[b.id].title));
        return results.slice(0, 8).map(r => r.id);
    }
}
