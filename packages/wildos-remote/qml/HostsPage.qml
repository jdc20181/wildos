import QtQuick
import QtQuick.Layouts
import WildOS.Remote
import WildOS.UI as UI

// The hosts (RemoteHosts.dc): terminal and desktop hosts together, by
// group, with All, Terminal and Desktop to narrow them. Above them, the
// desktops used last with the screen they ended on (RemoteStart.dc).
Item {
    id: page

    property string view: "hosts"
    property string search
    property string filter: "all"

    signal connectHost(string id)
    signal hostMenu(string id, Item item, real x, real y)
    signal newHost(string kind)

    readonly property string title: view === "favourites" ? qsTr("Favourites")
        : view.startsWith("group:") ? view.substring(6) : qsTr("All hosts")

    function matches(h) {
        if (view === "favourites" && !h.favourite) {
            return false;
        }
        if (view.startsWith("group:") && h.group !== view.substring(6)) {
            return false;
        }
        if (filter !== "all" && h.kind !== filter) {
            return false;
        }
        const q = search.trim().toLowerCase();
        if (q.length > 0) {
            const hay = (h.name + " " + h.address + " " + h.user + " " + (h.note || "")).toLowerCase();
            return hay.indexOf(q) >= 0;
        }
        return true;
    }
    // [{ name, hosts }], in the order of the groups, then hosts without one.
    readonly property var sections: {
        Hosts.revision;
        const out = [];
        const groups = Hosts.groups.slice();
        const all = Hosts.hosts.filter(h => page.matches(h));
        for (const g of groups) {
            const list = all.filter(h => h.group === g);
            if (list.length > 0) {
                out.push({ name: g, hosts: list });
            }
        }
        const loose = all.filter(h => !h.group || groups.indexOf(h.group) < 0);
        if (loose.length > 0) {
            out.push({ name: groups.length > 0 && out.length > 0 ? qsTr("Other") : "", hosts: loose });
        }
        return out;
    }
    readonly property int shownCount: {
        let n = 0;
        for (const s of sections) {
            n += s.hosts.length;
        }
        return n;
    }
    readonly property var recent: {
        Hosts.revision;
        if (filter === "terminal" || search.length > 0 || view !== "hosts") {
            return [];
        }
        return Hosts.hosts.filter(h => h.kind === "desktop" && h.lastUsed)
            .sort((a, b) => b.lastUsed.localeCompare(a.lastUsed)).slice(0, 3);
    }
    onRecentChanged: {
        for (const h of recent) {
            Hosts.probe(h.id);
        }
    }

    function lastUsedText(iso) {
        if (!iso) {
            return qsTr("Not used yet");
        }
        const d = new Date(iso);
        const now = new Date();
        const day = new Date(now.getFullYear(), now.getMonth(), now.getDate());
        const then = new Date(d.getFullYear(), d.getMonth(), d.getDate());
        const days = Math.round((day - then) / 86400000);
        if (days === 0) {
            return qsTr("Last used Today, %1").arg(d.toLocaleTimeString(Qt.locale(), Locale.ShortFormat));
        }
        if (days === 1) {
            return qsTr("Last used Yesterday");
        }
        if (days < 7) {
            return qsTr("Last used %1").arg(Qt.locale("en_GB").dayName(d.getDay(), Locale.ShortFormat));
        }
        return qsTr("Last used %1").arg(d.toLocaleDateString(Qt.locale(), Locale.ShortFormat));
    }
    function addressText(h) {
        return h.user ? qsTr("%1 on %2").arg(h.user).arg(h.address) : h.address;
    }
    function detailText(h) {
        if (h.note) {
            return h.note;
        }
        if (h.kind === "terminal") {
            const f = h.ssh.forwards || [];
            if (f.length > 0) {
                return qsTr("Forwards %1").arg(f.map(x => x.localPort).join(", "));
            }
            return h.ssh.identity ? qsTr("Key only") : "";
        }
        return "";
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            x: 36
            y: 22
            width: flick.width - 72
            spacing: 0

            Item {
                width: parent.width
                height: 44

                UI.Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.title
                    size: 26
                    font.weight: Font.Bold
                }
                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    Repeater {
                        model: [{ v: "all", t: qsTr("All") }, { v: "terminal", t: qsTr("Terminal") }, { v: "desktop", t: qsTr("Desktop") }]
                        Pill {
                            required property var modelData
                            text: modelData.t
                            checked: page.filter === modelData.v
                            onClicked: page.filter = modelData.v
                        }
                    }
                }
            }

            // Recent desktops, with their last screens.
            Column {
                visible: page.recent.length > 0
                width: parent.width
                spacing: 12
                topPadding: 8
                bottomPadding: 18

                Item {
                    width: parent.width
                    height: 24
                    UI.Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Recent")
                        size: 18
                        font.weight: Font.Bold
                    }
                    UI.Label {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Screens are from the end of your last session, kept on this PC only")
                        size: 13
                        color: UI.Style.inkMuted
                    }
                }
                Row {
                    spacing: 22
                    Repeater {
                        model: page.recent
                        RecentCard {
                            required property var modelData
                            width: (content.width - 44) / 3
                            host: modelData
                        }
                    }
                }
            }

            Repeater {
                model: page.sections
                Column {
                    id: section
                    required property var modelData
                    width: content.width
                    spacing: 12
                    topPadding: 6
                    bottomPadding: 16

                    Row {
                        visible: section.modelData.name.length > 0
                        spacing: 10
                        UI.Label {
                            text: section.modelData.name
                            size: 18
                            font.weight: Font.Bold
                        }
                        UI.Label {
                            anchors.baseline: parent.children[0].baseline
                            text: section.modelData.hosts.length === 1 ? qsTr("1 host") : qsTr("%1 hosts").arg(section.modelData.hosts.length)
                            size: 13
                            color: UI.Style.inkSubtle
                        }
                    }
                    Grid {
                        columns: Math.max(1, Math.floor((content.width + 14) / 340))
                        columnSpacing: 14
                        rowSpacing: 14
                        Repeater {
                            model: section.modelData.hosts
                            HostCard {
                                required property var modelData
                                width: (content.width - 14 * (parent.columns - 1)) / parent.columns
                                host: modelData
                            }
                        }
                    }
                }
            }

            // Nothing saved yet, or nothing matches.
            Column {
                visible: page.shownCount === 0
                width: parent.width
                topPadding: 80
                spacing: 14
                UI.Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Hosts.hosts.length === 0 ? qsTr("Connect to another computer") : qsTr("Nothing here matches")
                    size: 20
                    font.weight: Font.DemiBold
                }
                UI.Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, 520)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: Hosts.hosts.length === 0
                        ? qsTr("Add a Windows PC or server with Remote Desktop switched on, or a server you reach over SSH. You can also type an address like alex@web-01 in the box above.")
                        : qsTr("Try another name, or type an address above to connect to it.")
                    size: 14
                    color: UI.Style.inkMuted
                    lineHeight: 1.4
                }
                Row {
                    visible: Hosts.hosts.length === 0
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10
                    topPadding: 6
                    UI.Button {
                        text: qsTr("New desktop")
                        iconName: "monitor"
                        onClicked: page.newHost("desktop")
                    }
                    UI.Button {
                        text: qsTr("New SSH host")
                        iconName: "terminal"
                        onClicked: page.newHost("terminal")
                    }
                }
            }
        }
    }

    component Pill: Rectangle {
        id: pill
        property string text
        property bool checked
        signal clicked()
        width: pillLabel.implicitWidth + 26
        height: 30
        radius: 15
        color: checked ? UI.Style.accent : pillMouse.containsMouse ? UI.Style.surfaceHover : UI.Style.surfaceControl
        Accessible.role: Accessible.RadioButton
        Accessible.name: text
        Accessible.checked: checked
        UI.Label {
            id: pillLabel
            anchors.centerIn: parent
            text: pill.text
            size: 13
            font.weight: Font.DemiBold
            color: pill.checked ? UI.Style.textOnAccent : UI.Style.ink
        }
        MouseArea {
            id: pillMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: pill.clicked()
        }
    }

    component HostCard: Rectangle {
        id: card
        property var host
        readonly property bool desktop: host.kind === "desktop"
        readonly property color tint: desktop ? "#8fb4ff" : (host.ssh.colour || "#5fd39a")

        height: 74
        radius: 12
        color: cardMouse.containsMouse ? UI.Style.tone(0.07) : UI.Style.tone(0.045)
        border.width: 1
        border.color: UI.Style.tone(0.07)
        Accessible.role: Accessible.Button
        Accessible.name: host.name

        Rectangle {
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            width: 42
            height: 42
            radius: 10
            color: Qt.rgba(card.tint.r, card.tint.g, card.tint.b, 0.15)
            UI.Icon {
                anchors.centerIn: parent
                name: card.desktop ? "monitor" : "terminal"
                size: 19
                strokeWidth: 1.8
                color: card.tint
            }
        }
        Column {
            x: 70
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 70 - rightCol.width - 18
            spacing: 4
            Row {
                spacing: 6
                width: parent.width
                UI.Label {
                    text: card.host.name
                    size: 15
                    font.weight: Font.DemiBold
                    width: Math.min(implicitWidth, parent.width - 20)
                }
                UI.Icon {
                    visible: card.host.favourite === true
                    anchors.verticalCenter: parent.verticalCenter
                    name: "star"
                    size: 13
                    color: "#ffc46a"
                }
            }
            UI.Label {
                width: parent.width
                text: page.addressText(card.host)
                size: 13
                color: UI.Style.inkMuted
            }
        }
        Column {
            id: rightCol
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5
            Rectangle {
                anchors.right: parent.right
                width: kindLabel.implicitWidth + 14
                height: 20
                radius: 5
                color: UI.Style.tone(0.08)
                UI.Label {
                    id: kindLabel
                    anchors.centerIn: parent
                    text: card.desktop ? qsTr("Desktop") : qsTr("Terminal")
                    size: 11
                    font.weight: Font.DemiBold
                    color: UI.Style.inkMuted
                }
            }
            UI.Label {
                anchors.right: parent.right
                text: page.detailText(card.host)
                size: 12
                color: UI.Style.inkSubtle
            }
        }
        MouseArea {
            id: cardMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: Qt.PointingHandCursor
            onClicked: m => m.button === Qt.RightButton ? page.hostMenu(card.host.id, card, m.x, m.y) : page.connectHost(card.host.id)
        }
    }

    component RecentCard: Rectangle {
        id: recentCard
        property var host
        readonly property string reach: Hosts.reach[host.id] || ""
        readonly property url screen: { Hosts.revision; return Hosts.screenUrl(host.id); }

        height: 282
        radius: 14
        color: UI.Style.tone(0.045)
        border.width: 1
        border.color: UI.Style.tone(0.08)
        clip: true

        Rectangle {
            id: shot
            width: parent.width
            height: 194
            topLeftRadius: 14
            topRightRadius: 14
            color: Qt.rgba(13 / 255, 21 / 255, 48 / 255, 1)
            clip: true
            Image {
                anchors.fill: parent
                source: recentCard.screen
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                sourceSize.width: 720
            }
            UI.Icon {
                visible: recentCard.screen.toString().length === 0
                anchors.centerIn: parent
                name: "monitor"
                size: 40
                strokeWidth: 1.4
                color: UI.Style.tone(0.25)
            }
            Rectangle {
                visible: recentCard.reach.length > 0
                x: 12
                y: 12
                width: reachRow.implicitWidth + 16
                height: 24
                radius: 12
                color: Qt.rgba(8 / 255, 12 / 255, 28 / 255, 0.72)
                Row {
                    id: reachRow
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 7
                        height: 7
                        radius: 3.5
                        color: recentCard.reach === "online" ? "#5fd39a" : "#8b93a8"
                    }
                    UI.Label {
                        text: recentCard.reach === "online" ? qsTr("Online") : qsTr("Offline")
                        size: 12
                        font.weight: Font.DemiBold
                        color: "#eef1f8"
                    }
                }
            }
        }
        Column {
            x: 16
            y: shot.height + 14
            width: parent.width - 32 - connectButton.width
            spacing: 3
            Row {
                spacing: 6
                UI.Label {
                    text: recentCard.host.name
                    size: 16
                    font.weight: Font.DemiBold
                }
                UI.Icon {
                    visible: recentCard.host.favourite === true
                    anchors.verticalCenter: parent.verticalCenter
                    name: "star"
                    size: 13
                    color: "#ffc46a"
                }
            }
            UI.Label {
                width: parent.width
                text: page.addressText(recentCard.host)
                size: 13
                color: UI.Style.inkMuted
            }
            UI.Label {
                text: page.lastUsedText(recentCard.host.lastUsed)
                size: 12
                color: UI.Style.inkSubtle
            }
        }
        UI.Button {
            id: connectButton
            anchors.right: parent.right
            anchors.rightMargin: 16
            y: shot.height + 24
            kind: recentCard.reach === "offline" ? "secondary" : "primary"
            text: qsTr("Connect")
            onClicked: page.connectHost(recentCard.host.id)
        }
        MouseArea {
            anchors.fill: shot
            acceptedButtons: Qt.RightButton
            onClicked: m => page.hostMenu(recentCard.host.id, shot, m.x, m.y)
        }
    }
}
