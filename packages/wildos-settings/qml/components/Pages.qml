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
        "windowsapps", "accounts", "time", "accessibility", "privacy", "update"]

    readonly property var all: ({
        "home": { title: "Home", icon: "home", component: "HomePage",
            keywords: "start overview recommended" },

        "system": { title: "System", icon: "system", component: "CategoryPage",
            children: ["display", "sound", "notifications", "power", "storage", "multitasking", "about"],
            keywords: "system" },
        "display": { title: "Display", parent: "system", icon: "display", component: "DisplayPage", fallback: "kcm_kscreen",
            description: "Monitors, brightness, resolution and scale",
            keywords: "screen monitor resolution scale scaling zoom brightness refresh rate hz orientation rotate portrait hdr second screen multiple monitors projector arrange identify detect extend duplicate mirror main display primary variable refresh vrr freesync gsync" },
        "graphicsdriver": { title: "Graphics driver", parent: "display", icon: "chip", component: "GraphicsDriverPage",
            description: "Open driver or NVIDIA's own" },
        "nightlight": { title: "Night light", parent: "display", icon: "moon", component: "NightLightPage", fallback: "kcm_nightlight",
            description: "Warmer colours in the evening",
            keywords: "blue light warm colour color temperature evening sleep" },
        "sound": { title: "Sound", parent: "system", icon: "sound", component: "SoundPage", fallback: "kcm_pulseaudio",
            description: "Volume, output and input devices",
            keywords: "volume audio speakers headphones headset microphone mic input output mute balance mixer volume mixer app volume per app system sounds notification sounds windows apps hdmi bluetooth sound scheme sound theme startup sound start-up sound login sound beeps" },
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
            keywords: "snap layouts snap assist suggest fill alt tab task switcher windows side by side taskbar all desktops open windows" },
        "appnotifications": { title: "Notifications from apps", parent: "notifications", icon: "bell", component: "AppNotificationsPage",
            description: "Choose which apps can notify you",
            keywords: "app notifications notify sounds popups pop ups banners history notification list per app senders turn off mute" },
        "poweradvanced": { title: "More power settings", parent: "power", icon: "battery", component: "PowerAdvancedPage",
            description: "Lid, power button and dimming",
            keywords: "lid close closing laptop power button press dim dimming screen brightness battery critical low level hibernate shut down external monitor" },
        "desktops": { title: "Desktops", parent: "multitasking", icon: "desktops", component: "DesktopsPage",
            description: "Extra desktops for separate sets of windows",
            keywords: "virtual desktops workspaces add desktop new desktop rename remove task view" },
        "about": { title: "About", parent: "system", icon: "info", component: "AboutPage",
            description: "Device specifications, rename this PC",
            keywords: "device name rename pc computer name hostname specifications specs processor cpu memory ram version secure boot" },

        "devices": { title: "Bluetooth and devices", icon: "bluetooth", component: "CategoryPage",
            children: ["bluetooth", "mouse", "touchpad", "touchscreen", "pen", "keyboard", "printers", "gamecontrollers", "autoplay", "thunderbolt"],
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
        "keyboardadvanced": { title: "More keyboard options", parent: "keyboard", icon: "keyboard", component: "KeyboardAdvancedPage",
            description: "Keyboard model, compose key and Alt Gr",
            keywords: "compose key keyboard model alt gr special characters accents third level xkb options" },
        "touchscreen": { title: "Touchscreen", parent: "devices", icon: "display", component: "TouchscreenPage",
            description: "Turn touch on or off, choose its screen",
            keywords: "touch screen touchscreen touch input finger tap map screen" },
        "pen": { title: "Pen and tablet", parent: "devices", icon: "pencil", component: "PenPage",
            description: "Drawing tablets and pens, left-handed use",
            keywords: "pen stylus tablet drawing tablet graphics tablet wacom left handed map screen windows ink" },
        "gamecontrollers": { title: "Game controllers", parent: "devices", icon: "games", component: "GameControllersPage",
            description: "Gamepads and joysticks, test buttons and sticks",
            keywords: "game controller gamepad joystick xbox playstation controller test buttons sticks" },
        "autoplay": { title: "AutoPlay", parent: "devices", icon: "play", component: "AutoPlayPage",
            description: "What happens when you plug in a drive or memory card",
            keywords: "autoplay auto play usb stick memory card sd card removable drive mount automount" },
        "thunderbolt": { title: "Thunderbolt", parent: "devices", icon: "bolt", component: "ThunderboltPage",
            description: "Allow docks and other Thunderbolt devices",
            keywords: "thunderbolt usb4 dock docking station authorise authorize allow" },
        "printers": { title: "Printers and scanners", parent: "devices", icon: "printer", component: "PrintersPage", fallback: "kcm_printer_manager",
            description: "Add and manage printers",
            keywords: "printer printers print scanner add printer default printer test page" },
        "printersadvanced": { title: "More printer settings", parent: "printers", icon: "printer", component: "PrintersAdvancedPage",
            description: "Printers that need a driver, USB printers and print queues",
            keywords: "printer driver usb printer print queue pause printing cancel document paper size a4 letter two-sided duplex colour color black and white ppd" },

        "network": { title: "Network and internet", icon: "wifi", component: "NetworkPage",
            keywords: "wifi wi-fi wireless internet ethernet network airplane flight mode connect hotspot" },
        "networkadvanced": { title: "Advanced network settings", parent: "network", icon: "globe", component: "NetworkAdvancedPage",
            fallback: "kcm_networkmanagement",
            description: "VPN, proxy, IP addresses and saved networks",
            keywords: "vpn wireguard openvpn kill switch proxy dns encrypted dns over tls quad9 cloudflare mullvad ip address static ip mac address hardware address random private metered advanced connections saved networks forget" },
        // KDE's editor for what Wild OS has no page for: work Wi-Fi that
        // signs in with a username (802.1X), other kinds of VPN, bridges.
        // Search finds it.
        "networkconnections": { title: "All connection settings", parent: "networkadvanced", icon: "globe", window: "kcm_networkmanagement",
            description: "Every setting of every connection, in KDE's editor",
            keywords: "802.1x eap enterprise wpa2 enterprise eduroam certificate bond bridge vlan team openconnect anyconnect l2tp pptp ipsec" },

        "personalisation": { title: "Personalisation", icon: "palette", component: "PersonalisationPage",
            children: ["themes", "background", "colours", "lockscreen", "taskbar", "fonts"],
            keywords: "personalize personalise customise customize theme look light dark font fonts text size bigger" },
        "themes": { title: "Themes", parent: "personalisation", icon: "brush", component: "ThemesPage",
            description: "Wallpaper, light or dark, accent and glass in one go",
            keywords: "theme themes look style dawn orchard graphite overclock ember estuary gaming neon glass tint rgb lighting underglow glow keyboard lighting mouse lighting openrgb" },
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
            description: "Hiding the taskbar, Windows mode status",
            keywords: "taskbar panel auto hide autohide windows mode status virtual machine" },
        "fonts": { title: "Fonts", parent: "personalisation", icon: "font", component: "FontsPage",
            description: "Install and remove fonts",
            keywords: "font fonts typeface install font ttf otf typography" },

        "apps": { title: "Apps", icon: "apps", component: "CategoryPage",
            children: ["installedapps", "defaultapps", "startup", "oldapps"],
            keywords: "apps programs applications" },
        "installedapps": { title: "Installed apps", parent: "apps", icon: "list", component: "InstalledAppsPage",
            description: "Uninstall apps",
            keywords: "uninstall remove programs apps installed software add or remove" },
        "defaultapps": { title: "Default apps", parent: "apps", icon: "default-apps", component: "DefaultAppsPage", fallback: "kcm_componentchooser",
            description: "Browser, email and other default apps",
            keywords: "default browser email mail open with file associations" },
        "filetypes": { title: "Choose defaults by file type", parent: "defaultapps", icon: "document", component: "FileTypesPage",
            description: "Pick the app for .pdf, .docx and every other kind of file",
            keywords: "file type file types extension extensions mime associations open with pdf docx" },
        "startup": { title: "Startup", parent: "apps", icon: "startup", component: "StartupPage", fallback: "kcm_autostart",
            description: "Apps that start when you sign in",
            keywords: "startup autostart login sign in boot" },
        "oldapps": { title: "Old app compatibility", parent: "apps", icon: "app", component: "OldAppCompatibilityPage",
            description: "What older apps can see you type in other apps",
            keywords: "x11 xwayland legacy older apps keyboard shortcuts push to talk eavesdrop global shortcuts" },

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
            description: "Password, automatic sign-in and restarting apps",
            keywords: "password change password hint automatic sign in auto login autologin restart apps restore session reopen restartable" },
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
        "formatsadvanced": { title: "More format options", parent: "language", icon: "language", component: "FormatsPage",
            description: "Numbers, time, money and measurements separately",
            keywords: "number format currency format date format time format measurement units metric imperial paper size a4 letter file sizes" },
        "typing": { title: "Typing", parent: "time", icon: "pencil", component: "TypingPage",
            description: "Spell checking and your dictionary",
            keywords: "typing spelling spell check spellcheck dictionary misspelt misspelled words" },

        "accessibility": { title: "Accessibility", icon: "accessibility", component: "AccessibilityPage", fallback: "kcm_access",
            keywords: "accessibility ease of access text size bigger larger pointer cursor mouse pointer colour color size magnifier magnify zoom screen reader orca narrator read aloud high contrast flash visual alerts bell deaf hearing mono audio on-screen keyboard virtual keyboard touch keyboard sticky keys slow keys bounce keys filter keys outline windows window border focus ring focused window blur background windows inactive windows dim low vision stronger text glass transparency readable contrast" },
        "slowbouncekeys": { title: "Slow and bounce keys", parent: "accessibility", icon: "keyboard", component: "SlowBounceKeysPage",
            description: "Ignore quick or repeated presses",
            keywords: "slow keys bounce keys filter keys repeated presses double letters tremor hold key delay" },

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
        "apppermissionsadvanced": { title: "Everything an app can reach", parent: "apppermissions", icon: "sandbox", component: "AppPermissionsAdvancedPage",
            description: "Every sandbox permission, app by app",
            keywords: "flatpak permissions dbus d-bus environment variables devices sockets filesystem override reset advanced" },
        "screenlock": { title: "Screen lock", parent: "privacy", icon: "lock", component: "ScreenLockPage", fallback: "kcm_screenlocker",
            description: "Lock after a while and ask for your password",
            keywords: "lock screen timeout password sign in wake" },
        "searching": { title: "Searching files", parent: "privacy", icon: "search", component: "SearchingFilesPage",
            description: "File indexing and the folders search looks in",
            keywords: "search indexing index baloo find files excluded folders hidden files contents rebuild" },
        "activityhistory": { title: "Activity history", parent: "privacy", icon: "clock", component: "ActivityHistoryPage",
            description: "Recent files and clearing your history",
            keywords: "activity history recent files recently opened documents clear history forget" },

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
        // Advanced display was KDE's module; everything it offered that Wild
        // OS keeps is on Display now.
        "displayadvanced": "display",
        "kcm_networkmanagement": "networkadvanced",
        "kcm_wallpaper": "background",
        "kcm_lookandfeel": "personalisation",
        "kcm_colors": "colours",
        "kcm_about-distro": "about",
        "kcm_updates": "update",
        "kcm_cursortheme": "accessibility",
        "kcm_proxy": "networkadvanced",
        "kcm_soundtheme": "sound",
        // The volume mixer and every device are on Sound itself now.
        "soundadvanced": "sound",
        "kcm_pulseaudio": "sound",
        "kcm_energyinfo": "power",
        "kcm_kwin_virtualdesktops": "desktops",
        "kcm_smserver": "signin",
        "kcm_keyboard": "keyboard",
        "kcm_device_automounter": "autoplay",
        "kcm_gamecontroller": "gamecontrollers",
        "kcm_tablet": "pen",
        "kcm_touchscreen": "touchscreen",
        "kcm_bolt": "thunderbolt",
        "kcm_baloofile": "searching",
        "kcm_recentFiles": "activityhistory",
        "kcm_kwinxwayland": "oldapps",
        "kcm_virtualkeyboard": "accessibility",
        "kcm_access": "accessibility",
        "easeofaccess": "accessibility",
        "ease-of-access": "accessibility",
        // Text size in Personalisation is the Wild OS way to change the
        // fonts apps use; installing fonts has its own page.
        "kcm_fonts": "personalisation",
        "kcm_fontinst": "fonts",
        "kcmspellchecking": "typing",
        "kcm_filetypes": "filetypes"
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
