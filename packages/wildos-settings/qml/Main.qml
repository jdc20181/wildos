import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Templates as T
import WildOS.Settings
import WildOS.UI as UI

// The Settings window as Settings.dc.html draws it: one rounded glass sheet
// with its own 42px title bar (back button, "Settings", window buttons),
// navigation with the account and search on the left and the current page
// on the right. The title bar is drawn here rather than by KWin because the
// back button belongs in it; KWin's snap layouts find its maximise button by
// the app id org.wildos.settings (46px buttons on a 42px bar).
UI.AppWindow {
    id: window

    required property string startPage
    property string currentId: ""
    property var history: []
    property string pendingId: ""
    readonly property var current: Pages.page(currentId)
    property string currentKcm: ""
    property string pageError: ""
    readonly property bool currentIsKcm: currentKcm.length > 0
    readonly property bool canGoBack: history.length > 0 || (pageLoader.item !== null && pageLoader.item.depth !== undefined && pageLoader.item.depth > 1)

    width: 1180
    height: 760
    minimumWidth: 880
    minimumHeight: 560
    visible: true
    title: "Settings"
    deep: true
    transparency: System.transparency

    // For the D-Bus call from a second "wildos-settings --page x".
    function showPage(id) {
        openPage(id, false);
    }

    function openPage(id, fromHistory) {
        if (id === "signin-wallet") {
            Navigator.request = "wallet";
        }
        id = Pages.resolve(id);
        let p = Pages.page(id);
        if (!p || !Pages.exists(id)) {
            id = "home";
            p = Pages.page(id);
        }
        if (p.window) {
            KcmHost.openWindow(p.window, p.title);
            // Opened from the command line there is nothing behind the
            // window yet, so show where the setting lives.
            if (currentId.length === 0) {
                openPage(p.parent || "home");
            }
            return;
        }
        if (id === currentId) {
            return;
        }
        if (pageLoader.item && pageLoader.item.dirty) {
            pendingId = id;
            unsavedDialog.open();
            return;
        }
        if (!fromHistory && currentId.length > 0) {
            history = history.concat([currentId]);
        }
        currentId = id;
        pageLoader.sourceComponent = null;
        // A Wild OS page that cannot load says so, rather than quietly
        // showing KDE's module for the same setting in its place.
        let page = p.component ? Qt.createComponent("WildOS.Settings", p.component) : null;
        pageError = "";
        if (page && page.status === Component.Error) {
            console.warn(page.errorString());
            pageError = page.errorString();
            page = null;
        }
        currentKcm = page ? "" : (p.kcm || "");
        pageLoader.sourceComponent = page || (currentKcm.length > 0 ? kcmComponent : errorComponent);
        search.text = "";
    }

    // KDE labels its actions in title case with a trailing ellipsis ("Add
    // New..."); beside a Wild OS title they read as sentence case instead.
    // Words in capitals, such as VPN, keep them.
    function houseStyle(label) {
        const words = label.replace("&", "").replace(/(\.\.\.|…)$/, "").trim().split(" ");
        return words.map((word, i) => {
            if (i === 0 || (word.length > 1 && word === word.toUpperCase())) {
                return word;
            }
            return word.toLowerCase();
        }).join(" ");
    }

    function goBack() {
        if (pageLoader.item && pageLoader.item.back && pageLoader.item.back()) {
            return;
        }
        if (history.length > 0) {
            const previous = history[history.length - 1];
            history = history.slice(0, history.length - 1);
            openPage(previous, true);
        }
    }

    Component.onCompleted: {
        Navigator.window = window;
        openPage(startPage);
    }

    onClosing: close => {
        if (pageLoader.item && pageLoader.item.dirty) {
            close.accepted = false;
            pendingId = "";
            unsavedDialog.open();
        }
    }

    Shortcut {
        sequences: [StandardKey.Back, "Alt+Left"]
        onActivated: window.goBack()
    }

    Shortcut {
        sequences: [StandardKey.Find, "Ctrl+E"]
        onActivated: search.forceActiveFocus()
    }

    Component {
        id: errorComponent
        PageError {
            pageTitle: (Pages.page(window.currentId) || {}).title || ""
            detail: window.pageError
        }
    }

    Component {
        id: kcmComponent
        KcmPage {
            pluginId: window.currentKcm
        }
    }

    UI.TitleBar {
        id: titleBar
        width: parent.width
        title: "Settings"
        showBack: true
        backEnabled: window.canGoBack
        frame: window.frame
        onBackClicked: window.goBack()
    }

    Flickable {
        id: nav
        y: titleBar.height
        width: Metrics.navWidth
        height: parent.height - y
        contentHeight: navColumn.implicitHeight + 20
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        T.ScrollBar.vertical: UI.ScrollBar {}

        Column {
            id: navColumn
            x: 14
            y: 6
            width: Metrics.navWidth - 28
            spacing: 2

            T.AbstractButton {
                id: account
                width: parent.width
                height: 84
                padding: 0
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.role: Accessible.Button
                Accessible.name: "Account, " + System.userFullName
                onClicked: window.openPage("accounts")

                background: Rectangle {
                    radius: UI.Style.radiusSm
                    color: account.down ? UI.Style.surfacePressed : account.hovered ? UI.Style.surfaceHover : "transparent"
                    Behavior on color {
                        ColorAnimation { duration: UI.Style.hoverDuration }
                    }

                    UI.FocusRing {
                        visible: account.visualFocus
                        anchors.margins: -2
                        targetRadius: UI.Style.radiusSm
                    }
                }

                contentItem: Item {
                    Row {
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 14

                        UI.Avatar {
                            size: 60
                            source: System.userAvatar
                        }

                        UI.Label {
                            anchors.verticalCenter: parent.verticalCenter
                            width: navColumn.width - 60 - 14 - 16
                            text: System.userFullName
                            size: 15
                            font.weight: Font.DemiBold
                        }
                    }
                }
            }

            UI.SearchField {
                id: search
                width: parent.width
                placeholderText: "Find a setting"
                onAccepted: {
                    const found = Pages.search(text);
                    if (found.length > 0) {
                        window.openPage(found[0]);
                    }
                }
                onDownPressed: resultList.forceActiveFocus()
            }

            Item {
                width: 1
                height: 8
            }

            Repeater {
                model: Pages.navigation

                NavItem {
                    required property string modelData
                    iconName: Pages.page(modelData).icon
                    text: Pages.page(modelData).title
                    current: Pages.category(window.currentId) === modelData
                    onClicked: window.openPage(modelData)
                }
            }
        }
    }

    // Search results in the menu glass, as a drop-down list under the box.
    T.Popup {
        id: results
        x: 14
        y: nav.y + search.y + search.height + 6
        width: Metrics.navWidth - 28
        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
        padding: 6
        visible: search.text.trim().length > 0 && search.activeFocus || resultList.activeFocus
        closePolicy: T.Popup.NoAutoClose

        background: Rectangle {
            radius: UI.Style.radiusLg
            color: UI.Style.glassMenu
            border.width: 1
            border.color: UI.Style.borderMenu
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: UI.Style.shadowMenu
                shadowVerticalOffset: 12
                shadowBlur: 1.0
                blurMax: 32
            }
        }

        contentItem: Column {
            spacing: 2

            ListView {
                id: resultList
                width: parent.width
                height: contentHeight
                interactive: false
                spacing: 2
                model: Pages.search(search.text)
                currentIndex: 0
                keyNavigationEnabled: true
                Keys.onReturnPressed: window.openPage(model[currentIndex])
                Keys.onEnterPressed: window.openPage(model[currentIndex])
                Keys.onEscapePressed: search.forceActiveFocus()
                Keys.onUpPressed: event => {
                    if (currentIndex === 0) {
                        search.forceActiveFocus();
                    } else {
                        event.accepted = false;
                    }
                }

                delegate: T.AbstractButton {
                    id: result
                    required property string modelData
                    required property int index
                    readonly property var info: Pages.page(modelData)
                    readonly property bool lit: hovered || (resultList.activeFocus && ListView.isCurrentItem)
                    width: ListView.view.width
                    implicitHeight: Math.max(UI.Style.menuItemHeight, column.implicitHeight + 12)
                    padding: 0
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    Accessible.role: Accessible.ListItem
                    Accessible.name: info.title
                    onClicked: window.openPage(modelData)

                    background: Rectangle {
                        radius: UI.Style.radiusSm
                        color: result.down ? UI.Style.surfacePressed : result.lit ? UI.Style.surfaceHover : "transparent"
                    }

                    contentItem: Item {
                        UI.Icon {
                            x: 10
                            anchors.verticalCenter: parent.verticalCenter
                            name: result.info.icon || "info"
                            size: 16
                            strokeWidth: 1.8
                        }

                        Column {
                            id: column
                            x: 36
                            width: parent.width - x - 10
                            anchors.verticalCenter: parent.verticalCenter

                            UI.Label {
                                width: parent.width
                                text: result.info.title
                            }

                            UI.Label {
                                width: parent.width
                                visible: result.info.parent !== undefined
                                text: result.info.parent ? Pages.page(Pages.category(result.modelData)).title : ""
                                size: 12
                                color: UI.Style.inkMuted
                            }
                        }
                    }
                }
            }

            UI.Label {
                visible: resultList.count === 0
                width: parent.width
                height: UI.Style.menuItemHeight
                leftPadding: 10
                text: "No settings match “" + search.text.trim() + "”"
                color: UI.Style.inkMuted
            }
        }
    }

    Item {
        id: content
        x: Metrics.navWidth
        y: titleBar.height
        width: parent.width - Metrics.navWidth
        height: parent.height - y

        Row {
            id: header
            x: Metrics.contentLeft
            y: 8
            height: Math.max(34, UI.Style.px(28) + 6)
            spacing: 10

            // A trail too long for the window loses its first steps, as
            // Windows Settings does; the page itself always stays.
            readonly property int dropped: {
                const room = content.width - Metrics.contentLeft - Metrics.contentRight;
                const n = crumbs.count;
                const widths = [];
                let total = 0;
                for (let i = 0; i < n; ++i) {
                    const item = crumbs.itemAt(i);
                    widths.push(item ? item.implicitWidth : 0);
                    total += widths[i] + (i > 0 ? spacing : 0);
                }
                let d = 0;
                while (total > room && d < n - 1) {
                    total -= widths[d] + spacing;
                    ++d;
                }
                return d;
            }

            Repeater {
                id: crumbs
                model: {
                    const trail = Pages.trail(window.currentId).map(id => ({ id: id, title: Pages.page(id).title }));
                    const extra = pageLoader.item && pageLoader.item.subTitles ? pageLoader.item.subTitles : [];
                    return trail.concat(extra.map(t => ({ id: "", title: t })));
                }

                Row {
                    id: crumb
                    required property var modelData
                    required property int index
                    readonly property bool last: index === crumbs.count - 1
                    visible: index >= header.dropped
                    spacing: 10
                    anchors.verticalCenter: parent.verticalCenter

                    T.AbstractButton {
                        id: crumbButton
                        anchors.verticalCenter: parent.verticalCenter
                        width: crumbText.implicitWidth
                        height: header.height
                        padding: 0
                        enabled: !crumb.last && crumb.modelData.id.length > 0
                        hoverEnabled: true
                        focusPolicy: enabled ? Qt.StrongFocus : Qt.NoFocus
                        Accessible.role: crumb.last ? Accessible.Heading : Accessible.Link
                        Accessible.name: crumb.modelData.title
                        onClicked: window.openPage(crumb.modelData.id)

                        background: UI.FocusRing {
                            visible: crumbButton.visualFocus
                            targetRadius: UI.Style.radiusXs
                        }

                        contentItem: UI.Label {
                            id: crumbText
                            text: crumb.modelData.title
                            size: 28
                            font.weight: Font.DemiBold
                            font.letterSpacing: -0.28
                            color: crumb.last || crumbButton.hovered ? UI.Style.ink : UI.Style.inkMuted
                        }
                    }

                    UI.Icon {
                        visible: !crumb.last
                        anchors.verticalCenter: parent.verticalCenter
                        name: "chevron"
                        size: 18
                        strokeWidth: 2
                        color: UI.Style.inkMuted
                    }
                }
            }
        }

        Row {
            id: pageActions
            anchors.right: parent.right
            anchors.rightMargin: Metrics.contentRight
            y: header.y + (header.height - height) / 2
            spacing: 8
            visible: header.x + header.implicitWidth + 24 < x

            Repeater {
                model: pageLoader.item && pageLoader.item.actions ? pageLoader.item.actions : []

                UI.Button {
                    required property var modelData
                    text: window.houseStyle(modelData.text)
                    enabled: modelData.enabled
                    onClicked: modelData.trigger(this)
                }
            }
        }

        Loader {
            id: pageLoader
            y: header.y + header.height + (window.currentIsKcm ? 10 : 18)
            width: parent.width
            height: parent.height - y
            // Each page rises a little as it fades in, as Windows Settings does.
            transform: UI.Reveal {
                item: pageLoader
                key: window.currentId
                dy: UI.Style.travelMedium
            }
        }
    }

    UI.Dialog {
        id: unsavedDialog
        title: "Apply your changes?"
        text: "Some changes on this page are not applied yet."
        buttons: [
            UI.Button {
                text: "Apply"
                kind: "primary"
                onClicked: {
                    pageLoader.item.apply();
                    unsavedDialog.leave();
                }
            },
            UI.Button {
                text: "Discard changes"
                onClicked: {
                    pageLoader.item.discard();
                    unsavedDialog.leave();
                }
            },
            UI.Button {
                text: "Stay on this page"
                onClicked: unsavedDialog.close()
            }
        ]

        function leave() {
            close();
            if (window.pendingId.length > 0) {
                window.openPage(window.pendingId);
            } else {
                window.close();
            }
        }
    }
}
