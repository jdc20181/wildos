import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Templates as T
import WildOS.Store
import WildOS.UI as UI

// One app, as StoreApp.dc.html: icon, name and facts, the buttons that act on
// it, what it does, and what it can reach on this computer.
PageFlick {
    id: page

    property var store
    property string arg
    property string kind
    readonly property var app: Catalog.app(arg)
    readonly property bool working: app && (app.busy || app.queued)
    readonly property bool sandboxed: app && app.source === "flatpak"
    property bool expanded: false
    // Flathub answers within seconds; if nothing comes back (no network, no
    // metadata for this app) the page says so instead of checking for ever.
    property bool checkGaveUp: false

    Timer {
        running: page.sandboxed && page.app && page.app.permissions.length === 0 && !page.app.permissionsUnknown
        interval: 20000
        onTriggered: page.checkGaveUp = true
    }

    Component.onCompleted: if (app) app.loadDetails()

    Connections {
        target: page.app
        function onDataChanged() {
            page.app.loadDetails();
        }
    }

    UI.Button {
        id: backLink
        kind: "text"
        iconName: "chevron-left"
        iconSize: 15
        iconStroke: 2
        implicitHeight: 32
        leftPadding: 6
        rightPadding: 10
        text: page.store.backTitle()
        Accessible.name: "Back to " + page.store.backTitle()
        onClicked: page.store.back()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 24

        Rectangle {
            Layout.preferredWidth: 120
            Layout.preferredHeight: 120
            radius: 24
            color: Metrics.iconTile

            AppIcon {
                anchors.centerIn: parent
                size: 88
                source: page.app ? page.app.iconSource : ""
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            UI.Label {
                Layout.fillWidth: true
                text: page.app ? page.app.name : ""
                font.pixelSize: UI.Style.px(30)
                font.weight: Font.DemiBold
                font.letterSpacing: -0.3
                Accessible.role: Accessible.Heading
            }

            UI.Label {
                Layout.fillWidth: true
                text: page.app ? page.app.byline : ""
                font.pixelSize: UI.Style.px(14)
                color: Metrics.inkSub
            }

            Flow {
                Layout.fillWidth: true
                Layout.topMargin: 4
                spacing: 8

                // Every app here is free to download, so the chip says what
                // kind of free it is.
                UI.Chip {
                    granted: false
                    text: page.app && page.app.openSource ? "Open source" : "Free to use"
                }

                UI.Chip {
                    visible: page.sandboxed
                    iconName: "shield"
                    text: "Checked and sandboxed"
                }

                UI.Chip {
                    visible: page.app && page.app.chip.length > 0
                    granted: false
                    text: page.app ? page.app.chip : ""
                }
            }
        }

        Row {
            Layout.alignment: Qt.AlignVCenter
            spacing: 10

            ProgressPill {
                visible: page.working
                height: Math.max(40, implicitHeight)
                width: Math.max(180, implicitWidth)
                fontSize: 13
                app: page.app
            }

            UI.Button {
                size: "large"
                visible: !page.working && page.app && page.app.installed
                text: "Uninstall"
                onClicked: removeDialog.open()
            }

            UI.Button {
                size: "large"
                kind: !page.app.updateAvailable ? "primary" : "secondary"
                visible: !page.working && page.app && page.app.installed && page.app.canOpen
                text: "Open"
                onClicked: page.app.open()
            }

            UI.Button {
                size: "large"
                kind: "primary"
                visible: !page.working && page.app && page.app.installed && page.app.updateAvailable
                text: "Update"
                onClicked: page.app.update()
            }

            UI.Button {
                size: "large"
                kind: "primary"
                visible: !page.working && page.app && !page.app.installed
                enabled: page.app && page.app.source.length > 0
                text: "Install"
                onClicked: page.app.install()
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 24

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 18

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                UI.Label {
                    text: "About"
                    font.pixelSize: UI.Style.px(15)
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                }

                Item {
                    id: aboutClip
                    Layout.fillWidth: true
                    implicitHeight: page.expanded || !about.truncatedText ? about.implicitHeight : 6 * about.lineHeightPx
                    clip: true
                    // Fades the cut-off line instead of slicing it in half.
                    layer.enabled: !page.expanded && about.truncatedText
                    layer.effect: MultiEffect {
                        maskEnabled: true
                        maskSource: fadeMask
                        maskSpreadAtMin: 1.0
                    }

                    Text {
                        id: about
                        readonly property real lineHeightPx: font.pixelSize * 1.6
                        readonly property bool truncatedText: implicitHeight > 6 * lineHeightPx + 4
                        width: parent.width
                        text: page.app ? page.app.description : ""
                        textFormat: Text.RichText
                        wrapMode: Text.Wrap
                        color: Metrics.inkBody
                        font.family: UI.Style.fontFamily
                        font.pixelSize: UI.Style.px(14)
                        lineHeight: 1.6
                        onLinkActivated: link => Qt.openUrlExternally(link)
                    }
                }

                Item {
                    id: fadeMask
                    width: aboutClip.width
                    height: aboutClip.height
                    visible: false
                    layer.enabled: true

                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop { position: 0; color: "white" }
                            GradientStop { position: 0.75; color: "white" }
                            GradientStop { position: 1; color: "transparent" }
                        }
                    }
                }

                UI.LinkButton {
                    visible: about.truncatedText
                    Layout.leftMargin: -4
                    text: page.expanded ? "Show less" : "Read more"
                    onClicked: page.expanded = !page.expanded
                }
            }

            Rectangle {
                visible: page.app && page.app.swapTitle.length > 0
                Layout.fillWidth: true
                implicitHeight: swapRow.implicitHeight + 32
                radius: UI.Style.radiusMd
                color: UI.Style.accentTint
                border.width: 1
                border.color: UI.Style.accentBorder

                Row {
                    id: swapRow
                    x: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 36
                    spacing: 14

                    UI.Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "swap"
                        size: 20
                        strokeWidth: 2
                        color: UI.Style.accentText
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 34
                        spacing: 2

                        UI.Label {
                            width: parent.width
                            text: page.app ? page.app.swapTitle : ""
                            font.pixelSize: UI.Style.px(14)
                            font.weight: Font.DemiBold
                        }

                        UI.Label {
                            width: parent.width
                            text: page.app ? page.app.swapText : ""
                            font.pixelSize: UI.Style.px(12)
                            color: UI.Style.inkMuted
                            wrapMode: Text.Wrap
                            elide: Text.ElideNone
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                UI.Label {
                    text: "Status"
                    font.pixelSize: UI.Style.px(15)
                    font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                }

                RowGroup {
                    Layout.fillWidth: true

                    InfoRow {
                        visible: page.app && page.app.error.length > 0
                        warning: true
                        title: page.app && page.app.action === "remove" ? "Couldn't uninstall" : page.app && page.app.action === "update" ? "Couldn't update" : "Couldn't install"
                        subtitle: page.app ? page.app.error : ""

                        UI.Button {
                            size: "small"
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Dismiss"
                            onClicked: page.app.clearError()
                        }
                    }

                    InfoRow {
                        title: !page.app ? "" : page.working ? progressTitle.label
                            : page.app.installed ? "Installed" : "Not installed"
                        subtitle: {
                            if (!page.app) {
                                return "";
                            }
                            if (page.working) {
                                return page.app.queued ? "It starts when the apps before it finish" : "You can keep using the Store while it works";
                            }
                            if (page.app.installed) {
                                return page.app.updateAvailable ? "An update is ready" : "Up to date, updates arrive with your other apps";
                            }
                            return page.app.sizeText.length > 0 ? page.app.sizeText : "Free to install";
                        }

                        ProgressPill {
                            id: progressTitle
                            visible: false
                            app: page.app
                        }
                    }

                    InfoRow {
                        visible: page.app && page.app.installed && page.app.inStart
                        title: "Added to Start"
                        subtitle: page.app ? "In All apps under " + page.app.categoryLabel : ""
                    }

                    InfoRow {
                        visible: page.app && page.app.source.length > 0
                        title: page.sandboxed ? "Sandboxed app" : "Wild OS app"
                        subtitle: page.sandboxed ? "Runs in a sandbox, separate from your other apps" : "Updated along with the rest of Wild OS"

                        UI.LinkButton {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: page.app && !page.app.installed && !page.working && page.app.hasFlatpak && page.app.hasApt
                            text: page.sandboxed ? "Use the Wild OS version" : "Use the sandboxed version"
                            onClicked: page.app.useSource(page.sandboxed ? "apt" : "flatpak")
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 380
            Layout.maximumWidth: 380
            Layout.alignment: Qt.AlignTop
            spacing: 8

            UI.Label {
                text: page.sandboxed ? "What it can reach" : "Details"
                font.pixelSize: UI.Style.px(15)
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
            }

            RowGroup {
                visible: page.sandboxed
                Layout.fillWidth: true

                InfoRow {
                    visible: page.app && page.app.permissions.length === 0
                    minHeight: 54
                    readonly property bool failed: page.app && (page.app.permissionsUnknown || page.checkGaveUp)
                    title: failed ? "Couldn't check what it asks for" : "Checking what it asks for"
                    subtitle: failed ? "The app library can't be reached right now. Try again when you're online." : ""
                }

                Repeater {
                    model: page.sandboxed && page.app ? page.app.permissions : []

                    InfoRow {
                        id: permission
                        required property var modelData
                        minHeight: 54
                        title: modelData.label
                        last: false

                        UI.Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: permission.modelData.fixed ? "Always" : permission.modelData.allowed ? "On" : "Off"
                            font.pixelSize: UI.Style.px(12)
                            color: UI.Style.inkMuted
                        }

                        UI.Toggle {
                            visible: !permission.modelData.fixed
                            anchors.verticalCenter: parent.verticalCenter
                            checked: permission.modelData.allowed
                            locked: !permission.modelData.editable
                            lockedReason: "Change this once the app is installed"
                            Accessible.name: permission.modelData.label
                            onToggled: page.app.setPermission(permission.modelData.token, checked)
                        }
                    }
                }
            }

            RowGroup {
                visible: !page.sandboxed
                Layout.fillWidth: true

                InfoRow {
                    minHeight: 54
                    title: "Version"
                    visible: page.app && page.app.version.length > 0

                    UI.Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.app ? page.app.version : ""
                        font.pixelSize: UI.Style.px(12)
                        color: UI.Style.inkMuted
                    }
                }

                InfoRow {
                    minHeight: 54
                    title: "Developer"
                    visible: page.app && page.app.developer.length > 0

                    UI.Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, 200)
                        text: page.app ? page.app.developer : ""
                        font.pixelSize: UI.Style.px(12)
                        color: UI.Style.inkMuted
                    }
                }

                InfoRow {
                    minHeight: 54
                    title: "Licence"
                    subtitle: page.app ? page.app.licence : ""
                    visible: page.app && page.app.licence.length > 0

                    UI.Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, 200)
                        text: !page.app ? "" : page.app.openSource ? "Open source" : "Free to use"
                        font.pixelSize: UI.Style.px(12)
                        color: UI.Style.inkMuted
                    }
                }

                InfoRow {
                    minHeight: 54
                    title: "Website"
                    visible: page.app && page.app.homepage.length > 0

                    UI.LinkButton {
                        anchors.verticalCenter: parent.verticalCenter
                        size: 12
                        text: page.app ? page.app.homepage.replace(/^https?:\/\//, "").replace(/\/$/, "") : ""
                        onClicked: Qt.openUrlExternally(page.app.homepage)
                    }
                }
            }

            UI.Label {
                Layout.fillWidth: true
                Layout.topMargin: 4
                Layout.leftMargin: 2
                Layout.rightMargin: 2
                font.pixelSize: UI.Style.px(12)
                lineHeight: 1.5
                color: UI.Style.inkMuted
                wrapMode: Text.Wrap
                elide: Text.ElideNone
                text: !page.app ? ""
                    : !page.sandboxed ? page.app.name + " runs like any program on your computer, without a sandbox, so it can open any of your files."
                    : page.app.installed ? "Changes apply the next time " + page.app.name + " opens. " + page.app.name + " will ask if it ever needs something new."
                    : "This is what " + page.app.name + " asks for. You can change it once it's installed."
            }
        }
    }

    UI.Label {
        visible: shots.count > 0
        text: "Screenshots"
        font.pixelSize: UI.Style.px(15)
        font.weight: Font.DemiBold
        Accessible.role: Accessible.Heading
    }

    ListView {
        id: shots
        Layout.fillWidth: true
        Layout.topMargin: -18
        Layout.preferredHeight: 220
        visible: count > 0
        orientation: ListView.Horizontal
        spacing: 12
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: page.app ? page.app.screenshots : []
        ScrollBar.horizontal: UI.ScrollBar {}

        delegate: T.AbstractButton {
            id: shot
            required property var modelData
            required property int index
            height: 220
            width: modelData.width > 0 ? Math.round(220 * modelData.width / modelData.height) : 391
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            Accessible.role: Accessible.Button
            Accessible.name: modelData.caption || ("Screenshot " + (index + 1))
            onClicked: viewer.show(page.app.screenshots, index)

            background: Rectangle {
                radius: UI.Style.radiusMd
                color: UI.Style.surfaceRaised

                UI.FocusRing {
                    visible: shot.visualFocus
                    targetRadius: UI.Style.radiusMd
                }
            }

            contentItem: Item {
                Image {
                    id: shotImage
                    anchors.fill: parent
                    anchors.margins: 1
                    source: shot.modelData.thumb
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    smooth: true
                    visible: false
                }

                RoundedImage {
                    anchors.fill: parent
                    anchors.margins: 1
                    source: shotImage
                    radius: UI.Style.radiusMd - 1
                }

                Rectangle {
                    anchors.fill: parent
                    radius: UI.Style.radiusMd
                    color: shot.hovered ? UI.Style.tone(0.06) : "transparent"
                    border.width: 1
                    border.color: UI.Style.borderSubtle
                }
            }
        }
    }

    UI.Dialog {
        id: removeDialog
        title: page.app ? "Uninstall " + page.app.name + "?" : ""
        text: page.sandboxed ? "Your files stay where they are. Its settings stay too, in case you install it again."
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
                    page.app.remove();
                }
            }
        ]
    }

    ScreenshotViewer {
        id: viewer
    }
}
