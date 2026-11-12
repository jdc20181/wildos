import QtQuick
import QtQuick.Layouts
import WildOS.Welcome
import WildOS.UI as UI

// Setup step 1, from FirstRegion.dc.html (3.3). Language, formats and time
// zone start as the installer left them (the zone from the language's
// country when it set none); the keyboard is the installer's too and has no
// field here. "Find it for me" is not on the artboard: it is the opt-in
// lookup, offered under the time zone only while online, saying what it
// sends, and never run unless pressed.
StepPage {
    id: page

    signal next()

    property bool applying: false
    property string applyError: ""
    property bool changingFormats: false
    readonly property bool canLookUp: FirstBoot.wifi.online && FirstBoot.zoneSource !== "internet"

    gap: 18
    title: "Region and language"
    subtitle: "We picked these from the installer. Check they look right; you can change them later in Settings."
    note: applyError.length > 0 ? applyError : "Takes about 5 minutes. You can change everything later."
    noteColor: applyError.length > 0 ? UI.Style.warning : UI.Style.inkMuted
    Component.onCompleted: focusQuietly(continueButton)

    Connections {
        target: FirstBoot
        function onRegionApplied(ok, message) {
            // The Wi-Fi page applies a time zone found there too.
            if (!page.applying) {
                return;
            }
            page.applying = false;
            if (ok) {
                page.next();
            } else {
                page.applyError = message;
            }
        }
        function onRegionChanged() {
            page.applyError = "";
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 18
        rowSpacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            spacing: 6

            UI.Label {
                text: "Display language"
                font.weight: UI.Style.weightMedium
            }

            UI.ComboBox {
                Layout.fillWidth: true
                look: "field"
                model: FirstBoot.languages
                currentIndex: count > 0 ? indexOfValue(FirstBoot.language) : -1
                Accessible.name: "Display language"
                onActivated: FirstBoot.language = currentValue
            }

            UI.Label {
                Layout.fillWidth: true
                text: "Used for local content, like the Store and weather"
                size: 12
                color: UI.Style.inkMuted
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            spacing: 6

            UI.Label {
                text: "Time zone"
                font.weight: UI.Style.weightMedium
            }

            UI.ComboBox {
                Layout.fillWidth: true
                look: "field"
                model: FirstBoot.zones
                currentIndex: count > 0 ? indexOfValue(FirstBoot.timeZone) : -1
                Accessible.name: "Time zone"
                onActivated: FirstBoot.timeZone = currentValue
            }

            RowLayout {
                Layout.fillWidth: true
                visible: page.canLookUp || FirstBoot.zoneSource === "internet" || FirstBoot.lookupState === "failed"
                spacing: 8

                UI.Label {
                    Layout.fillWidth: true
                    text: FirstBoot.lookupState === "failed" ? "The online service didn't answer. Pick your time zone above."
                        : FirstBoot.zoneSource === "internet" ? "Found from your internet address."
                        : "Sends your internet address to an online service."
                    size: 12
                    color: FirstBoot.lookupState === "failed" ? UI.Style.danger : UI.Style.inkMuted
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                }

                UI.LinkButton {
                    visible: page.canLookUp
                    enabled: FirstBoot.lookupState !== "busy"
                    text: FirstBoot.lookupState === "busy" ? "Looking it up" : "Find it for me"
                    size: 12
                    onClicked: FirstBoot.findTimeZone()
                }
            }
        }
    }

    // How things will look: 16px 18px padding, 10px gaps, a 140px label
    // column and 13px text.
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: lookColumn.implicitHeight + 32
        radius: UI.Style.radiusMd
        color: UI.Style.surfaceRaised
        border.width: 1
        border.color: UI.Style.borderSubtle

        ColumnLayout {
            id: lookColumn
            x: 18
            y: 16
            width: parent.width - 36
            spacing: 10

            UI.Label {
                text: "How things will look"
                font.weight: UI.Style.weightSemiBold
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 6

                Repeater {
                    model: [
                        { label: "Date", key: "date" },
                        { label: "Time", key: "time" },
                        { label: "Numbers", key: "numbers" },
                        { label: "Currency", key: "currency" },
                        { label: "Measurements", key: "measurements" }
                    ]

                    delegate: Item {
                        required property var modelData
                        required property int index
                        Layout.row: index
                        Layout.column: 0
                        Layout.preferredWidth: 140
                        implicitHeight: label.implicitHeight

                        UI.Label {
                            id: label
                            text: parent.modelData.label
                            color: UI.Style.inkAlpha(0.70, 0.64)
                        }
                    }
                }

                Repeater {
                    model: ["date", "time", "numbers", "currency", "measurements"]

                    delegate: UI.Label {
                        required property string modelData
                        required property int index
                        Layout.row: index
                        Layout.column: 1
                        Layout.fillWidth: true
                        text: FirstBoot.preview[modelData] ?? ""
                    }
                }
            }

            UI.LinkButton {
                visible: !page.changingFormats
                Layout.leftMargin: -4
                text: "Change formats"
                onClicked: page.changingFormats = true
            }

            RowLayout {
                visible: page.changingFormats
                Layout.fillWidth: true
                spacing: 12

                UI.Label {
                    Layout.preferredWidth: 140
                    text: "Formats"
                    color: UI.Style.inkAlpha(0.70, 0.64)
                }

                UI.ComboBox {
                    Layout.preferredWidth: 300
                    model: FirstBoot.languages
                    currentIndex: count > 0 ? indexOfValue(FirstBoot.formats) : -1
                    Accessible.name: "Formats"
                    onActivated: FirstBoot.formats = currentValue
                }
            }
        }
    }

    buttons: [
        UI.Button {
            id: continueButton
            // A setting that could not be saved is not worth stopping for.
            text: page.applyError.length > 0 ? "Continue anyway" : "Continue"
            kind: "primary"
            busy: page.applying
            onClicked: {
                if (page.applyError.length > 0) {
                    page.next();
                    return;
                }
                page.applying = true;
                FirstBoot.applyRegion();
            }
        }
    ]
}
