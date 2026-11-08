/* Before we start: the first step (InstallerChecks.dc). check-computer, run
 * through the Installer singleton, looks at free space, mains power, Secure
 * Boot, BitLocker and Windows Fast Startup. Windows settings that need
 * changing are amber cards with the steps and a "Show me how" page; passed
 * checks are green rows. "Check again" in the footer runs it again. The
 * Windows settings block only installing alongside Windows, through
 * alongsideBlocked in global storage, which the disk page reads.
 *
 * Calamares' own requirements (welcomeq.conf) still block the install
 * outright, and show here as a red card.
 *
 * The language comes from the boot menu. This page also sets the time zone
 * from the language's country, through localeq (no page of its own); the
 * first start of the installed system lets people change it.
 */
import io.calamares.core 1.0
import io.calamares.ui 1.0
import WildOS.Installer
import WildOS.UI as UI

import QtQuick
import QtQuick.Layouts

WPage {
    id: page

    readonly property var requirements: config.requirementsModel
    readonly property bool blocked: !requirements.satisfiedMandatory
    readonly property var tz: Installer.locale
    readonly property var warnings: Installer.checks.filter(function(c) { return c.status === "warn"; })
    readonly property var passed: Installer.checks.filter(function(c) { return c.status !== "warn"; })
    readonly property int windowsWarnings: warnings.filter(function(c) {
        return c.id === "bitlocker" || c.id === "faststartup" || c.id === "dirty" || c.id === "space";
    }).length

    title: qsTr("Before we start")
    subtitle: {
        if (Installer.checking && Installer.checks.length === 0)
            return qsTr("Checking this computer.");
        if (windowsWarnings === 1)
            return qsTr("We checked this computer. One thing in Windows needs changing before Wild OS can safely sit alongside it.");
        if (windowsWarnings > 1)
            return qsTr("We checked this computer. %1 things in Windows need changing before Wild OS can safely sit alongside it.")
                .arg(windowsWarnings === 2 ? qsTr("Two") : qsTr("Three"));
        if (warnings.length > 0)
            return qsTr("We checked this computer. One thing needs your attention.");
        return qsTr("We checked this computer. Everything is ready for Wild OS.");
    }
    gap: 14
    topInset: -4

    Component.onCompleted: {
        Installer.welcome = config;
        Installer.runChecks();
    }

    Connections {
        target: Installer
        function onChecksChanged() {
            if (!Installer.checking)
                Global.insert("alongsideBlocked", Installer.alongsideBlocked);
        }
    }

    // The time zone follows the country of the boot language.
    readonly property string fallbackZone: "Europe/London"
    readonly property var countryFor: ({
        "en": "US", "de": "DE", "fr": "FR", "es": "ES", "it": "IT", "nl": "NL", "pt": "PT", "ja": "JP",
        "ko": "KR", "ru": "RU", "uk": "UA", "pl": "PL", "cs": "CZ", "sk": "SK", "sl": "SI", "hr": "HR",
        "sr": "RS", "hu": "HU", "ro": "RO", "bg": "BG", "el": "GR", "tr": "TR", "da": "DK", "nb": "NO",
        "sv": "SE", "fi": "FI", "et": "EE", "lt": "LT", "lv": "LV", "is": "IS", "he": "IL", "fa": "IR",
        "ar": "SA", "hi": "IN", "as": "IN", "bn": "IN", "ml": "IN", "mr": "IN", "th": "TH", "vi": "VN",
        "id": "ID", "ka": "GE", "be": "BY", "sq": "AL", "ca": "ES", "eu": "ES", "gl": "ES", "ast": "ES",
        "oc": "FR", "fur": "IT", "tg": "TJ", "si": "LK", "zh": "CN"
    })
    readonly property var zoneFor: ({
        "GB": "Europe/London", "IE": "Europe/Dublin", "US": "America/New_York", "CA": "America/Toronto",
        "AU": "Australia/Sydney", "NZ": "Pacific/Auckland", "ZA": "Africa/Johannesburg", "IN": "Asia/Kolkata",
        "DE": "Europe/Berlin", "AT": "Europe/Vienna", "CH": "Europe/Zurich", "FR": "Europe/Paris",
        "BE": "Europe/Brussels", "NL": "Europe/Amsterdam", "LU": "Europe/Luxembourg", "ES": "Europe/Madrid",
        "PT": "Europe/Lisbon", "BR": "America/Sao_Paulo", "AR": "America/Argentina/Buenos_Aires",
        "MX": "America/Mexico_City", "IT": "Europe/Rome", "DK": "Europe/Copenhagen", "NO": "Europe/Oslo",
        "SE": "Europe/Stockholm", "FI": "Europe/Helsinki", "IS": "Atlantic/Reykjavik", "EE": "Europe/Tallinn",
        "LV": "Europe/Riga", "LT": "Europe/Vilnius", "PL": "Europe/Warsaw", "CZ": "Europe/Prague",
        "SK": "Europe/Bratislava", "HU": "Europe/Budapest", "SI": "Europe/Ljubljana", "HR": "Europe/Zagreb",
        "RS": "Europe/Belgrade", "RO": "Europe/Bucharest", "BG": "Europe/Sofia", "GR": "Europe/Athens",
        "CY": "Asia/Nicosia", "TR": "Europe/Istanbul", "UA": "Europe/Kyiv", "BY": "Europe/Minsk",
        "RU": "Europe/Moscow", "AL": "Europe/Tirane", "IL": "Asia/Jerusalem", "IR": "Asia/Tehran",
        "SA": "Asia/Riyadh", "AE": "Asia/Dubai", "EG": "Africa/Cairo", "JP": "Asia/Tokyo", "KR": "Asia/Seoul",
        "CN": "Asia/Shanghai", "TW": "Asia/Taipei", "HK": "Asia/Hong_Kong", "VN": "Asia/Ho_Chi_Minh",
        "TH": "Asia/Bangkok", "ID": "Asia/Jakarta", "GE": "Asia/Tbilisi", "TJ": "Asia/Dushanbe",
        "LK": "Asia/Colombo", "NG": "Africa/Lagos", "MA": "Africa/Casablanca"
    })
    onTzChanged: {
        if (!tz)
            return;
        var conf = Global.value("localeConf");
        var lang = conf && conf.LANG ? String(conf.LANG).split(".")[0] : "";
        var country = lang.split("_")[1] || countryFor[lang.split("_")[0]] || "";
        tz.setCurrentLocation(zoneFor[country] || fallbackZone);
    }

    HelpWindow { id: help }

    // What Calamares itself refuses: too little memory or disk, not root.
    Rectangle {
        visible: page.blocked
        Layout.fillWidth: true
        implicitHeight: blockedText.implicitHeight + 26
        radius: UI.Style.radiusMd
        color: Qt.rgba(242 / 255, 139 / 255, 130 / 255, 0.08)
        border.width: 1
        border.color: Qt.rgba(242 / 255, 139 / 255, 130 / 255, 0.3)

        ColumnLayout {
            id: blockedText
            anchors.fill: parent
            anchors.margins: 13
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 4
            Text {
                Layout.fillWidth: true
                text: qsTr("Wild OS cannot be installed on this computer")
                color: UI.Style.ink
                font.family: UI.Style.fontFamily
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            Repeater {
                model: page.requirements
                delegate: Text {
                    required property bool satisfied
                    required property bool mandatory
                    required property string negatedText
                    Layout.fillWidth: true
                    visible: !satisfied && mandatory
                    text: negatedText
                    color: UI.Style.inkStatus
                    font.family: UI.Style.fontFamily
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Repeater {
        model: page.warnings
        delegate: Rectangle {
            required property var modelData
            Layout.fillWidth: true
            implicitHeight: card.implicitHeight + 26
            radius: UI.Style.radiusMd
            color: Qt.rgba(240 / 255, 180 / 255, 90 / 255, 0.08)
            border.width: 1
            border.color: Qt.rgba(240 / 255, 180 / 255, 90 / 255, 0.28)

            RowLayout {
                id: card
                anchors.fill: parent
                anchors.margins: 13
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    Layout.alignment: Qt.AlignTop
                    radius: 14
                    color: Qt.rgba(240 / 255, 180 / 255, 90 / 255, 0.2)
                    WIcon {
                        anchors.centerIn: parent
                        width: 15
                        height: 15
                        stroke: "#f5c27a"
                        strokeWidth: 2.4
                        path: "M12 7v6 M12 16.6v0.4"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Text {
                        Layout.fillWidth: true
                        text: modelData.title
                        color: UI.Style.ink
                        font.family: UI.Style.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.detail
                        color: Qt.rgba(232 / 255, 236 / 255, 248 / 255, 0.82)
                        font.family: UI.Style.fontFamily
                        font.pixelSize: 13
                        lineHeightMode: Text.FixedHeight
                        lineHeight: 19.5
                        wrapMode: Text.WordWrap
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        visible: (modelData.steps || []).length > 0
                        spacing: 2
                        Repeater {
                            model: modelData.steps || []
                            delegate: Text {
                                required property string modelData
                                required property int index
                                Layout.fillWidth: true
                                text: (index + 1) + ".  " + modelData
                                color: UI.Style.inkStatus
                                font.family: UI.Style.fontFamily
                                font.pixelSize: 12
                                lineHeightMode: Text.FixedHeight
                                lineHeight: 18
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
                UI.Button {
                    Layout.alignment: Qt.AlignTop
                    visible: (modelData.help || "") !== ""
                    text: qsTr("Show me how")
                    onClicked: help.openOn(modelData.help)
                }
            }
        }
    }

    Rectangle {
        visible: page.passed.length > 0
        Layout.fillWidth: true
        implicitHeight: rows.implicitHeight + 2
        radius: UI.Style.radiusMd
        color: UI.Style.surfaceRaised
        border.width: 1
        border.color: UI.Style.borderSubtle

        ColumnLayout {
            id: rows
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 1
            spacing: 0

            Repeater {
                model: page.passed
                delegate: Item {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    implicitHeight: Math.max(50, rowText.implicitHeight + 14)

                    Rectangle {
                        visible: index > 0
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: UI.Style.divider
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        spacing: 14
                        Rectangle {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            radius: 14
                            color: modelData.status === "ok" ? Qt.rgba(110 / 255, 210 / 255, 160 / 255, 0.18)
                                                             : UI.Style.accentChip
                            WIcon {
                                anchors.centerIn: parent
                                width: 15
                                height: 15
                                stroke: modelData.status === "ok" ? "#7fe0b0" : UI.Style.accentText
                                strokeWidth: modelData.status === "ok" ? 2.4 : 2
                                // Information: a circled i, as on the board.
                                path: modelData.status === "ok" ? "M5 12l5 5 9-10"
                                      : "M21 12a9 9 0 1 1-18 0a9 9 0 1 1 18 0 M12 11v5.5 M12 7.6v0.2"
                            }
                        }
                        ColumnLayout {
                            id: rowText
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: modelData.title
                                color: UI.Style.ink
                                font.family: UI.Style.fontFamily
                                font.pixelSize: 14
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.detail
                                color: UI.Style.inkMuted
                                font.family: UI.Style.fontFamily
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }

    Item { Layout.fillHeight: true }
}
