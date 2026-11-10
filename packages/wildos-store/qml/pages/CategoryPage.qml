import QtQuick
import QtQuick.Layouts
import WildOS.Store
import WildOS.UI as UI

// Create, Work, Play and Develop, as StoreCreate.dc.html: the title and a
// line about it, filters at the right, a feature card, the top picks
// numbered in three columns, then everything else in the category. The
// filters are freedesktop categories; an app is in a filter when it has
// any of `any`, and also one of `also` when that is given, and none of
// `not`.
AppGrid {
    id: page

    property var store
    property string arg
    property string kind
    property int selected: 0

    readonly property var spec: {
        switch (kind) {
        case "work": return {
            title: "Work",
            subtitle: "Documents, email, calls and money.",
            filters: [
                { label: "All", any: ["Office", "Email", "Finance", "ProjectManagement", "Calendar", "ContactManagement", "WordProcessor", "Spreadsheet", "Presentation", "VideoConference"] },
                { label: "Documents", any: ["WordProcessor", "Spreadsheet", "Presentation", "Publishing", "Office"], not: ["Email", "Finance", "ProjectManagement", "Calendar"] },
                { label: "Email and calls", any: ["Email", "VideoConference", "InstantMessaging", "Telephony", "Chat"] },
                { label: "Planning", any: ["ProjectManagement", "Calendar", "ContactManagement"] },
                { label: "Money", any: ["Finance"] }
            ],
            hero: {
                eyebrow: "Office files",
                photo: "qrc:/qt/qml/WildOS/Store/data/heroes/contours.jpg",
                photoY: 0.5,
                title: "Open Word, Excel and PowerPoint files",
                body: "LibreOffice opens them and saves back to them, so you can keep working with people who use Office.",
                apps: ["libreoffice"],
                open: "libreoffice"
            }
        };
        case "play": return {
            title: "Play",
            subtitle: "Free games, and Steam for the ones you bought on Windows.",
            filters: [
                { label: "All", any: ["Game"] },
                { label: "Action", any: ["ActionGame", "Shooter"] },
                { label: "Adventure", any: ["AdventureGame", "RolePlaying"] },
                { label: "Puzzles", any: ["LogicGame", "BlocksGame"] },
                { label: "Strategy", any: ["StrategyGame"] },
                { label: "Racing and sports", any: ["SportsGame"] },
                { label: "Board and cards", any: ["BoardGame", "CardGame"] }
            ],
            hero: {
                eyebrow: "Your Steam games",
                photo: "qrc:/qt/qml/WildOS/Store/data/heroes/waves-green.jpg",
                photoY: 0.5,
                title: "Bring your Steam library",
                body: "Steam plays most Windows games here. Sign in and your library is there.",
                apps: ["steam"],
                open: "steam"
            }
        };
        case "develop": return {
            title: "Develop",
            subtitle: "Code editors, version control and the tools around them.",
            filters: [
                { label: "All", any: ["Development"] },
                { label: "Editors", any: ["IDE", "TextEditor"], also: ["Development"] },
                { label: "Version control", any: ["RevisionControl"] },
                { label: "Debugging", any: ["Debugger", "Profiling"] },
                { label: "Databases", any: ["Database"] },
                { label: "Web", any: ["WebDevelopment"] }
            ],
            hero: {
                eyebrow: "Start here",
                photo: "qrc:/qt/qml/WildOS/Store/data/heroes/dusk-hills.jpg",
                photoY: 0.4,
                title: "Write code in a proper editor",
                body: "Free code editors, from quick and simple to full development environments.",
                apps: [],
                topApps: 2,
                filter: 1
            }
        };
        }
        return {
            title: "Create",
            subtitle: "Paint, edit photos, cut video, make music.",
            filters: [
                { label: "All", any: ["Graphics", "2DGraphics", "RasterGraphics", "VectorGraphics", "Photography", "3DGraphics", "AudioVideoEditing", "Music", "Midi", "Sequencer", "Mixer"] },
                { label: "Photo", any: ["Photography", "RasterGraphics"] },
                { label: "Drawing", any: ["2DGraphics", "VectorGraphics"] },
                { label: "Video", any: ["AudioVideoEditing"], not: ["Audio"] },
                { label: "Audio", any: ["Audio"], also: ["AudioVideoEditing", "Music", "Midi", "Sequencer", "Mixer", "Recorder"] },
                { label: "3D", any: ["3DGraphics"] }
            ],
            hero: {
                eyebrow: "Get started",
                photo: "qrc:/qt/qml/WildOS/Store/data/photos/create-timeline.jpg",
                photoY: 0.5,
                title: "Make your first video tonight",
                body: "Kdenlive and Shotcut are free and never put a watermark on your videos.",
                apps: ["kdenlive", "org.shotcut.Shotcut"],
                filter: 3
            }
        };
    }

    function keysFor(f) {
        let keys = Catalog.matching(f.any);
        if (f.also) {
            const also = new Set(Catalog.matching(f.also));
            keys = keys.filter(k => also.has(k));
        }
        if (f.not) {
            const not = new Set(Catalog.matching(f.not));
            keys = keys.filter(k => !not.has(k));
        }
        return keys;
    }

    readonly property var allKeys: Catalog.catalogRevision >= 0 ? keysFor(spec.filters[Math.min(selected, spec.filters.length - 1)]) : []
    readonly property var topKeys: allKeys.slice(0, 9)
    readonly property var heroKeys: {
        if (Catalog.catalogRevision < 0) {
            return [];
        }
        if (spec.hero.topApps) {
            return keysFor(spec.filters[0]).slice(0, spec.hero.topApps);
        }
        return spec.hero.apps.map(id => Catalog.resolve(id)).filter(k => k.length > 0);
    }

    keys: allKeys.slice(9)
    onActivated: key => store.openApp(key)

    header: Column {
        width: page.innerWidth
        spacing: Metrics.sectionGap

        PageTitle {
            width: parent.width
            title: page.spec.title
            subtitle: page.spec.subtitle

            Repeater {
                model: page.spec.filters

                FilterPill {
                    required property var modelData
                    required property int index
                    text: modelData.label
                    selected: page.selected === index
                    onClicked: page.selected = index
                }
            }
        }

        HeroCard {
            width: parent.width
            height: 220
            eyebrow: page.spec.hero.eyebrow
            title: page.spec.hero.title
            body: page.spec.hero.body
            photo: page.spec.hero.photo || ""
            photoY: page.spec.hero.photoY !== undefined ? page.spec.hero.photoY : 0.5
            keys: page.heroKeys
            shadeStrength: 0.85
            onClicked: {
                const hero = page.spec.hero;
                if (hero.open) {
                    const key = Catalog.resolve(hero.open);
                    if (key.length > 0) {
                        page.store.openApp(key);
                    }
                } else if (hero.filter !== undefined) {
                    page.selected = hero.filter;
                }
            }
        }

        Column {
            width: parent.width

            SectionHeader {
                width: parent.width
                title: "Top picks"
            }

            AppList {
                width: parent.width
                keys: page.topKeys
                numbered: true
                onActivated: key => page.store.openApp(key)
            }

            UI.Label {
                visible: page.topKeys.length === 0
                topPadding: 16
                text: Catalog.ready ? "Nothing here yet." : "Getting the list of apps"
                color: Metrics.inkSecondary
            }
        }

        SectionHeader {
            visible: page.keys.length > 0
            width: parent.width
            title: page.selected > 0 ? "More in " + page.spec.filters[page.selected].label : page.kind === "play" ? "More games" : "More apps"
        }
    }
}
