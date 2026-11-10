import QtQuick
import QtQuick.Layouts
import WildOS.Store
import WildOS.UI as UI

// Updates, StoreUpdates.dc.html: the apps with an update and what changed,
// the ones updated lately, and at the side the overnight switch and the
// way to Wild OS's own updates, which live in Settings.
PageFlick {
    id: page

    property var store
    property string arg
    property string kind
    readonly property var keys: Catalog.catalogRevision + Catalog.stateRevision >= 0 ? Catalog.updates() : []
    readonly property var recent: Catalog.catalogRevision + Catalog.stateRevision >= 0 ? Catalog.recentlyUpdated(5) : []
    readonly property bool updatingAll: Catalog.updateAllProgress >= 0
    readonly property bool sideBySide: width - 2 * Metrics.sidePadding > 760

    PageTitle {
        title: "Updates"
        subtitle: {
            const n = page.keys.length;
            if (n > 0) {
                const count = n === 1 ? "1 update" : n + " updates";
                return Catalog.updatesBytes >= 0 ? count + ", " + Catalog.sizeText(Catalog.updatesBytes) + " in total" : count;
            }
            return Catalog.checking ? "Checking for updates."
                : Catalog.checkResult === "failed" ? "Couldn't check every app for updates. Check you're online and try again."
                : Catalog.checkResult === "ok" ? "Your apps are up to date. Checked at " + Catalog.checkedAt + "."
                : "Your apps are up to date.";
        }

        Row {
            visible: page.updatingAll
            spacing: 10
            height: 34

            UI.Label {
                anchors.verticalCenter: parent.verticalCenter
                text: "Updating"
                size: 13
                font.weight: Font.DemiBold
            }

            Ring {
                anchors.verticalCenter: parent.verticalCenter
                size: 34
                mode: Catalog.updateAllProgress > 0 ? "progress" : "spin"
                progress: Math.max(0, Catalog.updateAllProgress)
                Accessible.role: Accessible.ProgressBar
                Accessible.name: "Updating all apps, " + Math.max(0, Catalog.updateAllProgress) + "%"
            }
        }

        PillButton {
            visible: !page.updatingAll && page.keys.length > 0
            big: true
            filled: true
            enabled: !Catalog.busy
            text: "Update all"
            onClicked: Catalog.updateAll()
        }

        PillButton {
            visible: !page.updatingAll && page.keys.length === 0
            big: true
            enabled: !Catalog.checking
            text: Catalog.checking ? "Checking" : "Check for updates"
            onClicked: Catalog.refresh()
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: page.sideBySide ? 2 : 1
        columnSpacing: 40
        rowSpacing: Metrics.sectionGap

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Metrics.sectionGap

            ColumnLayout {
                visible: page.keys.length > 0
                Layout.fillWidth: true
                spacing: 0

                SectionHeader {
                    title: "Available"
                }

                Repeater {
                    model: page.keys

                    Item {
                        id: entry
                        required property string modelData
                        required property int index
                        readonly property var app: Catalog.app(modelData)
                        Layout.fillWidth: true
                        implicitHeight: Math.max(60, notes.implicitHeight, button.implicitHeight + 2) + 32

                        Rectangle {
                            visible: entry.index > 0
                            width: parent.width
                            height: 1
                            color: Metrics.divider
                        }

                        AppIcon {
                            id: icon
                            y: 16
                            size: 60
                            source: entry.app ? entry.app.iconSource : ""

                            TapHandler {
                                onTapped: page.store.openApp(entry.modelData)
                            }
                        }

                        Column {
                            id: notes
                            anchors.left: icon.right
                            anchors.leftMargin: 16
                            anchors.right: button.left
                            anchors.rightMargin: 16
                            y: 16
                            spacing: 4

                            Row {
                                width: parent.width
                                spacing: 10

                                UI.Label {
                                    id: name
                                    text: entry.app ? entry.app.name : ""
                                    size: 15
                                    font.weight: Font.DemiBold
                                    width: Math.min(implicitWidth, parent.width)
                                }

                                UI.Label {
                                    y: name.baselineOffset - baselineOffset
                                    width: Math.max(0, parent.width - name.width - 10)
                                    text: {
                                        const a = entry.app;
                                        if (!a) {
                                            return "";
                                        }
                                        const parts = [];
                                        if (a.newVersion.length > 0) {
                                            parts.push("Version " + a.newVersion);
                                        }
                                        if (a.updateSizeText.length > 0) {
                                            parts.push(a.updateSizeText);
                                        }
                                        return parts.join(", ");
                                    }
                                    size: 12.5
                                    color: Metrics.inkTertiary
                                }
                            }

                            Text {
                                width: parent.width
                                visible: text.length > 0
                                textFormat: Text.StyledText
                                wrapMode: Text.Wrap
                                font.family: UI.Style.fontFamily
                                font.pixelSize: UI.Style.px(13.5)
                                lineHeight: 1.5
                                color: Metrics.inkSecondary
                                linkColor: UI.Style.accent
                                // The note is plain text from the catalogue;
                                // only the More link is markup.
                                text: {
                                    const a = entry.app;
                                    if (!a) {
                                        return "";
                                    }
                                    const note = a.releaseNote.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
                                    return note.length > 0 ? note + " <a href=\"more\">More</a>" : "";
                                }
                                onLinkActivated: page.store.openApp(entry.modelData)

                                HoverHandler {
                                    cursorShape: parent.hoveredLink.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                                }
                            }
                        }

                        StoreButton {
                            id: button
                            anchors.right: parent.right
                            y: 18
                            app: entry.app
                        }
                    }
                }
            }

            ColumnLayout {
                visible: page.recent.length > 0
                Layout.fillWidth: true
                spacing: 0

                SectionHeader {
                    title: "Updated recently"
                }

                Repeater {
                    model: page.recent

                    AppListRecent {
                        required property string modelData
                        Layout.fillWidth: true
                        appKey: modelData
                        onActivated: key => page.store.openApp(key)
                    }
                }
            }
        }

        ColumnLayout {
            Layout.preferredWidth: page.sideBySide ? 320 : -1
            Layout.maximumWidth: page.sideBySide ? 320 : Number.POSITIVE_INFINITY
            Layout.fillWidth: !page.sideBySide
            Layout.alignment: Qt.AlignTop
            spacing: 16

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: autoColumn.implicitHeight + 40
                radius: Metrics.radiusCard
                color: Metrics.card

                Column {
                    id: autoColumn
                    x: 20
                    y: 20
                    width: parent.width - 40
                    spacing: 10

                    Item {
                        width: parent.width
                        height: Math.max(autoTitle.implicitHeight, autoToggle.implicitHeight)

                        UI.Label {
                            id: autoTitle
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - autoToggle.width - 12
                            text: "Update automatically"
                            size: 15
                            font.weight: Font.Bold
                        }

                        UI.Toggle {
                            id: autoToggle
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            checked: Catalog.autoUpdate
                            Accessible.name: "Update automatically"
                            onToggled: Catalog.autoUpdate = checked
                        }
                    }

                    UI.Label {
                        width: parent.width
                        text: "Apps update overnight while your PC is plugged in. An app that's open waits until you close it."
                        size: 13
                        lineHeight: 1.5
                        color: Metrics.inkSecondary
                        wrapMode: Text.Wrap
                        elide: Text.ElideNone
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: systemColumn.implicitHeight + 40
                radius: Metrics.radiusCard
                color: Metrics.card

                Column {
                    id: systemColumn
                    x: 20
                    y: 20
                    width: parent.width - 40
                    spacing: 10

                    Row {
                        width: parent.width
                        spacing: 10

                        UI.Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: "shield-check"
                            size: 20
                            color: Metrics.success
                        }

                        UI.Label {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 30
                            text: "Wild OS updates"
                            size: 15
                            font.weight: Font.Bold
                        }
                    }

                    UI.Label {
                        width: parent.width
                        text: "System updates live in Settings, so this list is only your apps."
                        size: 13
                        lineHeight: 1.5
                        color: Metrics.inkSecondary
                        wrapMode: Text.Wrap
                        elide: Text.ElideNone
                    }

                    StoreLink {
                        x: -4
                        size: 13
                        text: "Open Settings"
                        onClicked: Catalog.openSettings("update")
                    }
                }
            }
        }
    }
}
