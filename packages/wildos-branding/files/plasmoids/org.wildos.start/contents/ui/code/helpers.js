.pragma library

// The design names apps by what they do, not by their project names, so the
// pinned grid reads "Files" rather than "Dolphin". Keyed by desktop file id.
const friendlyNames = {
    "org.kde.dolphin": "Files",
    "brave-browser": "Brave",
    "mullvad-browser": "Mullvad Browser",
    "org.kde.discover": "Store",
    "org.wildos.files": "Files",
    "org.wildos.store": "Store",
    "systemsettings": "Settings",
    "org.wildos.settings": "Settings",
    "org.wildos.terminal": "Terminal",
    "org.kde.konsole": "Terminal",
    "org.kde.kcalc": "Calculator",
    "org.kde.kwrite": "Text editor",
    "org.kde.okular": "Documents",
    "org.kde.gwenview": "Pictures",
    "org.keepassxc.KeePassXC": "Password manager",
};

// Apps that make up the system itself. Windows does not offer to uninstall
// these from Start either.
const coreApps = ["org.wildos.files", "org.wildos.store", "org.kde.dolphin", "org.kde.discover", "systemsettings", "org.wildos.settings", "org.wildos.terminal", "org.kde.konsole"];

function isCoreApp(favoriteId) {
    return coreApps.includes(storageId(favoriteId));
}

// KDE settings modules as a Windows user would know them: the page title,
// the Settings category it lives in, and the Wild OS Settings page to open.
// Modules missing from this list are developer or Plasma internals, or have
// no Wild OS Settings page yet (phone link, firewall and the like), and are
// left out of search rather than landing on a page that does not show them.
const settingsPages = {
    "kcm_kscreen": ["Display", "System", "display"],
    "kcm_nightlight": ["Night light", "System › Display", "nightlight"],
    "kcm_pulseaudio": ["Sound", "System", "sound"],
    "kcm_soundtheme": ["System sounds", "System › Sound", "sound"],
    "kcm_notifications": ["Notifications", "System", "notifications"],
    "kcm_powerdevilprofilesconfig": ["Power and battery", "System", "power"],
    "kcm_energyinfo": ["Battery usage", "System › Power and battery", "power"],
    "kcm_about-distro": ["About", "System", "about"],
    "kcm_updates": ["Update", "Settings", "update"],
    "kcm_bluetooth": ["Bluetooth", "Bluetooth and devices", "bluetooth"],
    "kcm_mouse": ["Mouse", "Bluetooth and devices", "mouse"],
    "kcm_touchpad": ["Touchpad", "Bluetooth and devices", "touchpad"],
    "kcm_keyboard": ["Keyboard", "Bluetooth and devices", "keyboard"],
    "kcm_printer_manager": ["Printers and scanners", "Bluetooth and devices", "printers"],
    "kcm_networkmanagement": ["Network and internet", "Settings", "network"],
    "kcm_proxy": ["Proxy", "Network and internet", "networkadvanced"],
    "kcm_wallpaper": ["Background", "Personalisation", "background"],
    "kcm_lookandfeel": ["Themes", "Personalisation", "personalisation"],
    "kcm_colors": ["Colours", "Personalisation", "colours"],
    "kcm_fonts": ["Fonts", "Personalisation", "fonts"],
    "kcm_cursortheme": ["Mouse pointer", "Accessibility", "accessibility"],
    "kcm_access": ["Accessibility", "Settings", "accessibility"],
    "kcm_virtualkeyboard": ["On-screen keyboard", "Accessibility", "accessibility"],
    "kcm_screenlocker": ["Lock screen", "Personalisation", "lockscreen"],
    "kcm_componentchooser": ["Default apps", "Apps", "defaultapps"],
    "kcm_autostart": ["Startup apps", "Apps", "startup"],
    "kcm_users": ["Your info", "Accounts", "users"],
    "kcm_regionandlang": ["Language and region", "Time and language", "language"],
    "kcm_clock": ["Date and time", "Time and language", "datetime"],
    "kcm_flatpak": ["App permissions", "Privacy and security", "apppermissions"],
    "kcm_app-permissions": ["App permissions", "Privacy and security", "apppermissions"],
    "kcm_kwin_virtualdesktops": ["Desktops", "System › Multitasking", "desktops"],
    "kcm_device_automounter": ["AutoPlay", "Bluetooth and devices", "autoplay"],
    "kcm_gamecontroller": ["Game controllers", "Bluetooth and devices", "gamecontrollers"],
    "kcm_tablet": ["Pen and tablet", "Bluetooth and devices", "pen"],
    "kcm_touchscreen": ["Touchscreen", "Bluetooth and devices", "touchscreen"],
    "kcm_bolt": ["Thunderbolt", "Bluetooth and devices", "thunderbolt"],
    "kcm_fontinst": ["Fonts", "Personalisation", "fonts"],
    "kcmspellchecking": ["Typing", "Time and language", "typing"],
    "kcm_filetypes": ["Default apps by file type", "Apps › Default apps", "filetypes"],
    "kcm_smserver": ["Sign-in options", "Accounts", "signin"],
    "kcm_baloofile": ["Searching files", "Privacy and security", "searching"],
    "kcm_recentFiles": ["Activity history", "Privacy and security", "activityhistory"],
    "kcm_kwinxwayland": ["Old app compatibility", "Apps", "oldapps"],
    "kcm_access": ["Accessibility", "Settings", "accessibility"],
    "kcm_virtualkeyboard": ["On-screen keyboard", "Accessibility", "accessibility"],
};

function settingsPage(url) {
    const text = String(url ?? "");
    const id = text.substring(text.lastIndexOf("/") + 1).replace(/\.desktop$/, "");
    const entry = settingsPages[id];
    return entry ? { title: entry[0], category: entry[1], page: entry[2], kcm: id } : null;
}

// Single quotes for a POSIX shell argument.
function shellQuote(text) {
    return "'" + String(text).replace(/'/g, "'\\''") + "'";
}

function storageId(favoriteId) {
    if (!favoriteId) {
        return "";
    }
    let id = String(favoriteId);
    if (id.startsWith("applications:")) {
        id = id.substring("applications:".length);
    }
    if (id.endsWith(".desktop")) {
        id = id.substring(0, id.length - ".desktop".length);
    }
    return id;
}

function appName(favoriteId, display) {
    const id = storageId(favoriteId);
    return friendlyNames[id] ?? display ?? "";
}

// Wild OS hides file extensions in search and recents.
function withoutExtension(name) {
    const text = String(name ?? "");
    const dot = text.lastIndexOf(".");
    if (dot <= 0 || text.length - dot > 6) {
        return text;
    }
    return text.substring(0, dot);
}

function localPath(url) {
    const text = String(url ?? "");
    if (!text.startsWith("file://")) {
        return "";
    }
    return decodeURIComponent(text.substring("file://".length));
}

// "/home/alex/Documents/Invoices/x.pdf" becomes "Documents › Invoices".
function folderTrail(url, homePath) {
    let path = localPath(url);
    if (path.length === 0) {
        return "";
    }
    if (path.endsWith("/")) {
        path = path.substring(0, path.length - 1);
    }
    const parent = path.substring(0, path.lastIndexOf("/"));
    const home = String(homePath ?? "").replace(/\/$/, "");
    let parts;
    if (home.length > 0 && (parent === home || parent.startsWith(home + "/"))) {
        parts = ["Home"].concat(parent.substring(home.length).split("/").filter(p => p.length > 0));
        if (parts.length > 1) {
            parts.shift();
        }
    } else {
        parts = parent.split("/").filter(p => p.length > 0);
        if (parts.length === 0) {
            return "Computer";
        }
    }
    if (parts.length > 3) {
        parts = parts.slice(parts.length - 3);
    }
    return parts.join(" › ");
}

const imageExtensions = ["jpg", "jpeg", "png", "webp", "gif", "bmp", "avif", "heic", "tif", "tiff"];

function isImage(url) {
    const path = localPath(url).toLowerCase();
    const dot = path.lastIndexOf(".");
    return dot > 0 && imageExtensions.includes(path.substring(dot + 1));
}

// KDE ships some action labels in title case. The design system wants sentence
// case, so known labels are rewritten and common words lowered in the rest,
// leaving names such as "Brave" alone.
const actionLabels = {
    "openParentFolder": "Open file location",
    "_kicker_fileItem_properties": "Properties",
    "forget": "Remove from list",
    "forgetAll": "Clear this list",
    "addToTaskManager": "Pin to taskbar",
    "addToDesktop": "Add to desktop",
    "addToPanel": "Add to panel",
};

const lowerWords = ["a", "an", "and", "all", "as", "containing", "copy", "document", "documents", "edit", "file", "files", "folder", "for", "from", "history", "in", "incognito", "location", "mode", "new", "of", "on", "open", "private", "recent", "remove", "run", "settings", "show", "tab", "terminal", "the", "this", "to", "window", "with"];

function sentenceCase(text) {
    const words = String(text ?? "").replace(/…$/, "").split(" ");
    return words.map((word, i) => {
        if (i === 0) {
            return word;
        }
        return lowerWords.includes(word.toLowerCase()) ? word.toLowerCase() : word;
    }).join(" ");
}

function actionLabel(action) {
    return actionLabels[action.actionId] ?? sentenceCase(action.text);
}

function groupThousands(plain) {
    if (plain.includes("e")) {
        return plain;
    }
    const negative = plain.startsWith("-");
    const [whole, fraction] = (negative ? plain.substring(1) : plain).split(".");
    const grouped = whole.replace(/\B(?=(\d{3})+(?!\d))/g, ",");
    return (negative ? "-" : "") + grouped + (fraction ? "." + fraction : "");
}

// A small calculator that runs entirely here. Plasma's calculator runner can
// download exchange rates, and search must never reach the network.
function calculate(input) {
    let text = String(input ?? "").trim();
    if (text.startsWith("=")) {
        text = text.substring(1);
    }
    text = text.replace(/×/g, "*").replace(/÷/g, "/").replace(/,/g, "").replace(/\s+/g, "");
    if (!/^[0-9+\-*\/^%().]+$/.test(text) || !/[0-9]/.test(text) || !/[+\-*\/^%]/.test(text.replace(/^[-+]/, ""))) {
        return null;
    }

    let pos = 0;
    const peek = () => text[pos];
    const fail = () => { throw new Error("syntax"); };

    function number() {
        const start = pos;
        while (pos < text.length && /[0-9.]/.test(text[pos])) {
            pos++;
        }
        const slice = text.substring(start, pos);
        if (slice.length === 0 || (slice.match(/\./g) || []).length > 1) {
            fail();
        }
        return parseFloat(slice);
    }
    function primary() {
        if (peek() === "(") {
            pos++;
            const value = sum();
            if (peek() !== ")") {
                fail();
            }
            pos++;
            return value;
        }
        if (peek() === "-") {
            pos++;
            return -primary();
        }
        if (peek() === "+") {
            pos++;
            return primary();
        }
        return number();
    }
    function postfix() {
        let value = primary();
        while (peek() === "%") {
            pos++;
            value = value / 100;
        }
        return value;
    }
    function power() {
        const base = postfix();
        if (peek() === "^") {
            pos++;
            return Math.pow(base, power());
        }
        return base;
    }
    function product() {
        let value = power();
        while (peek() === "*" || peek() === "/") {
            const op = text[pos++];
            const rhs = power();
            value = op === "*" ? value * rhs : value / rhs;
        }
        return value;
    }
    function sum() {
        let value = product();
        while (peek() === "+" || peek() === "-") {
            const op = text[pos++];
            const rhs = product();
            value = op === "+" ? value + rhs : value - rhs;
        }
        return value;
    }

    try {
        const value = sum();
        if (pos !== text.length || !isFinite(value)) {
            return null;
        }
        const plain = String(parseFloat(value.toPrecision(12)));
        return {
            expression: String(input).trim().replace(/^=/, "").trim(),
            value: groupThousands(plain),
            plain: plain,
        };
    } catch (e) {
        return null;
    }
}
