import QtQuick
import QtQuick.Window
import WildOS.Store
import WildOS.UI as UI

// The Store window as Store.dc.html draws it: its own 52px title bar with
// search in it, sections down the left, the current page on the right.
UI.AppWindow {
    id: root

    required property string startApp
    required property string startSearch
    required property string startPage

    property var current: ({ page: "", arg: "", section: "home" })
    property var history: []
    property var pending: null
    // The design's column, wider when larger text needs it.
    readonly property real navWidth: {
        let widest = 0;
        for (let i = 0; i < nav.children.length; ++i) {
            widest = Math.max(widest, nav.children[i].implicitWidth);
        }
        return Math.max(Metrics.navWidth, widest + 24);
    }

    width: 1220
    height: 760
    minimumWidth: 880
    minimumHeight: 560
    visible: true
    title: "Store"
    // Store.dc.html draws the deeper window glass, rgba(18,22,40,0.86).
    deep: true

    function titleOf(entry) {
        switch (entry.page) {
        case "home": return "Home";
        case "apps": return entry.arg ? Catalog.categoryLabel(entry.arg) : "Apps";
        case "games": return entry.arg ? Catalog.categoryLabel(entry.arg) : "Games";
        case "windows": return "Windows apps";
        case "updates": return "Updates";
        case "installed": return "Installed";
        case "search": return "Search";
        case "app": {
            const a = Catalog.app(entry.arg);
            return a ? a.name : "App";
        }
        }
        return "Home";
    }

    function load(entry) {
        current = entry;
        if (entry.page !== "search") {
            search.text = "";
        }
        const urls = {
            home: "pages/HomePage.qml",
            apps: "pages/BrowsePage.qml",
            games: "pages/BrowsePage.qml",
            windows: "pages/WindowsAppsPage.qml",
            updates: "pages/UpdatesPage.qml",
            installed: "pages/InstalledPage.qml",
            search: "pages/SearchPage.qml",
            app: "pages/AppPage.qml"
        };
        pageLoader.setSource(urls[entry.page] || urls.home, { store: root, arg: entry.arg, kind: entry.page });
    }

    function open(page, arg) {
        const isSection = ["home", "apps", "games", "windows", "updates", "installed"].indexOf(page) >= 0;
        const entry = { page: page, arg: arg || "", section: isSection ? page : current.section };
        if (current.page === entry.page && current.arg === entry.arg) {
            return;
        }
        // Picking a section starts a fresh trail; going deeper remembers the
        // way back.
        if (isSection && !(page === current.section && (page === "apps" || page === "games") && current.page === page)) {
            history = [];
        } else if (current.page.length > 0) {
            history = history.concat([current]);
        }
        load(entry);
    }

    function openApp(key) {
        open("app", key);
    }

    // The menu every app tile, card and row shares (AppMenuArea).
    function showAppMenu(key, item, x, y) {
        const a = Catalog.app(key);
        if (!a) {
            return;
        }
        const entries = [{ text: "See details", icon: "info", run: () => root.openApp(key) }];
        if (a.installed && a.canOpen) {
            entries.push({ text: "Open", icon: "open", run: () => a.open() });
        }
        if (a.installed && a.updateAvailable && !a.busy && !a.queued) {
            entries.push({ text: "Update", icon: "update", run: () => a.update() });
        }
        if (!a.installed) {
            entries.push({ text: "Install", icon: "download", enabled: a.source.length > 0 && !a.busy && !a.queued, run: () => a.install() });
        }
        entries.push({ separator: true });
        entries.push({ text: "Copy link", icon: "link", enabled: a.appstreamId.length > 0, run: () => root.copyLink(a) });
        if (a.installed) {
            entries.push({ separator: true });
            entries.push({ text: "Uninstall", icon: "delete", danger: true, enabled: !a.busy && !a.queued, run: () => root.askRemove(a) });
        }
        appMenu.entries = entries;
        appMenu.popup(item, x, y);
    }

    // appstream: links open the app's page in the Store, here or on any
    // Linux desktop.
    function copyLink(app) {
        clipboard.text = "appstream://" + app.appstreamId;
        clipboard.selectAll();
        clipboard.copy();
        toast.note("Link copied");
    }

    property var removing: null
    function askRemove(app) {
        removing = app;
        removeDialog.open();
    }

    function back() {
        if (history.length === 0) {
            open("home");
            return;
        }
        const previous = history[history.length - 1];
        history = history.slice(0, history.length - 1);
        load(previous);
    }

    function backTitle() {
        return history.length > 0 ? titleOf(history[history.length - 1]) : "Home";
    }

    function runSearch(text) {
        const t = text.trim();
        if (t.length === 0) {
            if (current.page === "search") {
                back();
            }
            return;
        }
        if (current.page === "search") {
            current = { page: "search", arg: t, section: current.section };
            if (pageLoader.item) {
                pageLoader.item.arg = t;
            }
        } else {
            open("search", t);
        }
    }

    // From the command line, D-Bus and appstream: links.
    function openTarget(app, text, page) {
        if (!Catalog.ready) {
            pending = { app: app, text: text, page: page };
            return;
        }
        if (app && app.length > 0) {
            const key = Catalog.resolve(app);
            if (key.length > 0) {
                openApp(key);
            } else {
                // Not in either catalogue: show what a search finds instead
                // of an empty page.
                search.text = app.replace(/^appstream:\/*/, "");
                runSearch(search.text);
            }
        } else if (text && text.length > 0) {
            search.text = text;
            runSearch(text);
        } else if (page && page.length > 0) {
            open(page);
        }
    }

    Component.onCompleted: {
        open(startPage);
        if (startApp.length > 0 || startSearch.length > 0) {
            openTarget(startApp, startSearch, "");
        }
    }

    Connections {
        target: Catalog
        function onReadyChanged() {
            if (Catalog.ready && root.pending) {
                const p = root.pending;
                root.pending = null;
                root.openTarget(p.app, p.text, p.page);
            }
        }
        function onInstalled(key) {
            toast.showFor(key, "installed");
        }
        function onFailed(key, message) {
            toast.showFor(key, "failed", message);
        }
    }

    Shortcut {
        sequences: [StandardKey.Back, "Alt+Left"]
        onActivated: root.back()
    }

    Shortcut {
        sequences: [StandardKey.Find, "Ctrl+E"]
        onActivated: search.forceActiveFocus()
    }

    // The design's border sits outside the 1220 by 760 content box.
    Item {
        id: inner
        anchors.fill: parent
        anchors.margins: root.maximised ? 0 : 1

        UI.TitleBar {
            id: titleBar
            width: parent.width
            height: Metrics.titleBarHeight
            frame: root.frame
            buttons.buttonHeight: Metrics.titleBarHeight

            Image {
                id: logo
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: 20
                sourceSize: Qt.size(40, 40)
                source: "qrc:/qt/qml/WildOS/Store/data/org.wildos.store.svg"
            }

            UI.Label {
                id: appName
                anchors.left: logo.right
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: "Store"
                color: root.active ? UI.Style.ink : UI.Style.inkMuted
            }

            UI.SearchField {
                id: search
                pill: true
                anchors.left: appName.right
                anchors.leftMargin: 12 + 160
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(440, parent.width - x - 16)
                height: 34
                placeholderText: "Search apps, or the Windows app you used before"
                Accessible.name: "Search apps"
                Accessible.description: placeholderText
                onTextEdited: searchDelay.restart()
                // The clear button and Escape empty the box without an edit.
                onTextChanged: if (text.length === 0) searchDelay.restart()
                onAccepted: {
                    searchDelay.stop();
                    root.runSearch(text);
                }
            }

            Timer {
                id: searchDelay
                interval: 250
                onTriggered: root.runSearch(search.text)
            }
        }

        Column {
            id: nav
            y: titleBar.height + 8
            x: 12
            width: root.navWidth - 24
            spacing: 2

            NavItem {
                iconName: "home"
                text: "Home"
                current: root.current.section === "home"
                onClicked: root.open("home")
            }

            NavItem {
                iconName: "apps"
                text: "Apps"
                current: root.current.section === "apps"
                onClicked: root.open("apps")
            }

            NavItem {
                iconName: "games"
                text: "Games"
                current: root.current.section === "games"
                onClicked: root.open("games")
            }

            NavItem {
                iconName: "windowsapps"
                text: "Windows apps"
                current: root.current.section === "windows"
                onClicked: root.open("windows")
            }

            Item {
                width: parent.width
                height: 17

                Rectangle {
                    x: 12
                    y: 8
                    width: parent.width - 24
                    height: 1
                    color: UI.Style.tone(0.08)
                }
            }

            NavItem {
                iconName: "update"
                text: "Updates"
                badge: Catalog.updateCount
                current: root.current.section === "updates"
                onClicked: root.open("updates")
            }

            NavItem {
                iconName: "installed"
                text: "Installed"
                current: root.current.section === "installed"
                onClicked: root.open("installed")
            }
        }

        Loader {
            id: pageLoader
            x: root.navWidth
            y: titleBar.height
            width: parent.width - root.navWidth
            height: parent.height - titleBar.height
            clip: true
            focus: true
            // Each page rises a little as it fades in, as the Microsoft Store does.
            transform: UI.Reveal {
                item: pageLoader
                key: pageLoader.item
                dy: UI.Style.travelMedium
            }
        }

        Toast {
            id: toast
            anchors.horizontalCenter: pageLoader.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            onOpenRequested: key => root.openApp(key)
        }
    }

    UI.Menu {
        id: appMenu
    }

    UI.Dialog {
        id: removeDialog
        title: root.removing ? "Uninstall " + root.removing.name + "?" : ""
        text: root.removing && root.removing.source === "flatpak"
            ? "Your files stay where they are. Its settings stay too, in case you install it again."
            : "Your files stay where they are. Wild OS will ask for the administrator password."
        buttons: [
            UI.Button {
                text: "Cancel"
                onClicked: removeDialog.close()
            },
            UI.Button {
                kind: "primary"
                text: "Uninstall"
                onClicked: {
                    removeDialog.close();
                    root.removing.remove();
                }
            }
        ]
    }

    // QML has no clipboard of its own; a hidden text field copies for it.
    TextInput {
        id: clipboard
        visible: false
    }
}
