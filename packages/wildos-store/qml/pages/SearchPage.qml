import QtQuick
import QtQuick.Layouts
import WildOS.Store
import WildOS.UI as UI

// Search results, StoreSearch.dc.html. Typing a Windows app's name
// ("photoshop") puts the free apps that do its job first, under a note
// saying what they replace, with the way to the Windows program itself
// after them. Everything else that matches follows.
AppGrid {
    id: page

    property var store
    property string arg
    property string kind
    readonly property var swapKeys: Catalog.catalogRevision >= 0 ? Catalog.swapMatches(arg) : []
    readonly property string windowsName: {
        if (swapKeys.length === 0) {
            return "";
        }
        const a = Catalog.app(swapKeys[0]);
        return a ? a.insteadOf : "";
    }

    keys: {
        const all = Catalog.catalogRevision >= 0 ? Catalog.search(arg) : [];
        return all.filter(k => swapKeys.indexOf(k) < 0);
    }
    onActivated: key => store.openApp(key)

    header: Column {
        width: page.innerWidth
        spacing: Metrics.sectionGap
        bottomPadding: page.swapKeys.length > 0 ? 0 : 10

        PageTitle {
            width: parent.width
            title: "Results for “" + page.arg + "”"
            subtitle: !Catalog.ready ? "Getting the list of apps"
                : page.keys.length + page.swapKeys.length === 0 ? "Nothing matches. Try the name of the Windows app you used, or what you want to do, like “edit photos”." : ""
        }

        Rectangle {
            visible: page.windowsName.length > 0
            width: parent.width
            height: bannerText.implicitHeight + 36
            radius: Metrics.radiusCard
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: UI.Style.accentAlpha(0.16, 0.12) }
                GradientStop { position: 1; color: Qt.rgba(196 / 255, 122 / 255, 138 / 255, UI.Style.dark ? 0.12 : 0.09) }
            }
            border.width: 1
            border.color: UI.Style.accentAlpha(0.22, 0.20)

            Rectangle {
                id: bannerIcon
                x: 22
                anchors.verticalCenter: parent.verticalCenter
                width: 44
                height: 44
                radius: 12
                color: UI.Style.accentAlpha(0.18, 0.14)

                StoreIcon {
                    anchors.centerIn: parent
                    name: "swap-windows"
                    size: 22
                    color: UI.Style.accent
                }
            }

            Column {
                id: bannerText
                anchors.left: bannerIcon.right
                anchors.leftMargin: 18
                anchors.right: parent.right
                anchors.rightMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                UI.Label {
                    width: parent.width
                    text: "Looking for " + page.windowsName + "?"
                    size: 16
                    font.weight: Font.Bold
                }

                UI.Label {
                    width: parent.width
                    text: "These free apps do the same job on Wild OS."
                    size: 13.5
                    lineHeight: 1.45
                    color: Metrics.inkSecondary
                    wrapMode: Text.Wrap
                    elide: Text.ElideNone
                }
            }
        }

        Column {
            visible: page.swapKeys.length > 0
            width: parent.width

            SectionHeader {
                width: parent.width
                title: "Free apps that do the job"
            }

            AppList {
                width: parent.width
                keys: page.swapKeys
                showNotes: true
                onActivated: key => page.store.openApp(key)
            }
        }

        Column {
            visible: page.swapKeys.length > 0
            width: parent.width

            SectionHeader {
                width: parent.width
                title: "Windows apps"
            }

            WindowsModeRow {
                width: parent.width
            }
        }

        SectionHeader {
            visible: page.swapKeys.length > 0 && page.keys.length > 0
            width: parent.width
            title: "More results"
        }
    }
}
