import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import WildOS.Store
import WildOS.UI as UI

// Home, as Store.dc.html: the welcome banner, the Windows swaps and what is
// already installed, then more to browse below the fold.
PageFlick {
    id: page

    property var store
    property string arg
    property string kind

    readonly property var essentials: Catalog.revision >= 0 ? Catalog.essentials() : []
    readonly property int essentialsLeft: {
        let n = 0;
        for (const k of essentials) {
            const a = Catalog.app(k);
            if (a && !a.installed) {
                n++;
            }
        }
        return n;
    }
    readonly property bool essentialsBusy: {
        for (const k of essentials) {
            const a = Catalog.app(k);
            if (a && (a.busy || a.queued)) {
                return true;
            }
        }
        return false;
    }

    Item {
        id: hero
        Layout.fillWidth: true
        Layout.preferredHeight: Math.max(210, heroText.implicitHeight + 60)

        Item {
            id: heroArt
            anchors.fill: parent
            visible: false
            layer.enabled: true

            // object-position: center 62%, as the design crops it.
            Image {
                readonly property real scaledHeight: width * 1334 / 2000
                width: Math.max(parent.width, parent.height * 2000 / 1334)
                height: scaledHeight
                x: (parent.width - width) / 2
                y: -(scaledHeight - parent.height) * 0.62
                source: "qrc:/qt/qml/WildOS/Store/data/hero.jpg"
                sourceSize.width: 2000
                asynchronous: true
            }

            Rectangle {
                anchors.fill: parent
                color: Metrics.heroShade
            }
        }

        Rectangle {
            id: heroMask
            anchors.fill: parent
            radius: Metrics.radiusHero
            visible: false
            layer.enabled: true
        }

        MultiEffect {
            anchors.fill: parent
            source: heroArt
            maskEnabled: true
            maskSource: heroMask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }

        Column {
            id: heroText
            x: 36
            y: 36
            width: Math.min(520, parent.width - 72)
            spacing: 10

            UI.Label {
                text: "NEW TO WILD OS"
                font.pixelSize: UI.Style.px(12)
                font.weight: Font.DemiBold
                font.letterSpacing: 0.72
                color: Metrics.eyebrow
            }

            UI.Label {
                width: parent.width
                text: "The free apps that replace what you used on Windows"
                color: Metrics.onPhoto
                font.pixelSize: UI.Style.px(30)
                font.weight: Font.DemiBold
                font.letterSpacing: -0.3
                lineHeight: 36 * UI.Style.textScale
                lineHeightMode: Text.FixedHeight
                wrapMode: Text.Wrap
                elide: Text.ElideNone
                Accessible.role: Accessible.Heading
            }

            Row {
                topPadding: 6
                spacing: 10

                UI.Button {
                    size: "large"
                    kind: "primary"
                    enabled: page.essentialsLeft > 0 && !page.essentialsBusy
                    text: page.essentialsBusy ? "Installing the essentials" : page.essentialsLeft === 0 && page.essentials.length > 0 ? "Essentials installed" : "Install the essentials"
                    onClicked: Catalog.installEssentials()
                }

                UI.Button {
                    size: "large"
                    textColor: Metrics.onPhoto
                    // A glass button on the photograph, light in either scheme.
                    background: Rectangle {
                        radius: 10
                        color: Metrics.onPhotoAlpha(parent.down ? 0.18 : parent.hovered ? 0.14 : 0.10)
                        border.width: 1
                        border.color: Metrics.onPhotoAlpha(0.30)

                        UI.FocusRing {
                            visible: parent.parent.visualFocus
                            targetRadius: 10
                        }
                    }
                    text: "See the list"
                    onClicked: page.store.open("windows")
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 12

        SectionHeader {
            title: "Swap your Windows apps"
            linkText: "See all"
            onLinkClicked: page.store.open("windows")
        }

        SwapGrid {
            keys: Catalog.revision >= 0 ? Catalog.homeSwaps() : []
            onActivated: key => page.store.openApp(key)
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 12
        visible: installedGrid.keys.length > 0

        SectionHeader {
            title: "Already on this computer"
            linkText: "How we check apps"
            onLinkClicked: checkDialog.open()
        }

        TileGrid {
            id: installedGrid
            keys: Catalog.revision >= 0 ? Catalog.homeInstalled() : []
            maxItems: 4
            columns: 4
            onActivated: key => page.store.openApp(key)
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 12

        SectionHeader {
            title: "Popular apps"
            linkText: "See all apps"
            onLinkClicked: page.store.open("apps")
        }

        TileGrid {
            keys: Catalog.revision >= 0 ? Catalog.featured("popular") : []
            onActivated: key => page.store.openApp(key)
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 12

        SectionHeader {
            title: "Make something"
        }

        TileGrid {
            keys: Catalog.revision >= 0 ? Catalog.featured("create") : []
            onActivated: key => page.store.openApp(key)
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 12

        SectionHeader {
            title: "Play"
            linkText: "See all games"
            onLinkClicked: page.store.open("games")
        }

        TileGrid {
            keys: Catalog.revision >= 0 ? Catalog.featured("games") : []
            onActivated: key => page.store.openApp(key)
        }
    }

    UI.Dialog {
        id: checkDialog
        title: "How we check apps"
        text: "Apps marked Checked and sandboxed come from Flathub, the app library Wild OS uses. Each one is reviewed before it is listed, and runs in a sandbox that only reaches the folders and devices you allow on its page.\n\nApps from Wild OS packages are built and signed by the Wild OS maintainers, and get security fixes with your other updates. They run like any program on your computer, so their page says so."
        buttons: [
            UI.Button {
                kind: "primary"
                text: "Got it"
                onClicked: checkDialog.close()
            }
        ]
    }
}
