import QtQuick
import QtQuick.Layouts
import WildOS.UI as UI

// What shows when a Wild OS page can't load. It used to fall back to KDE's
// own module for the setting, which put KDE's look in front of people and
// hid the fault; now the fault is visible and can be reported.
PageScroll {
    id: page

    property string pageTitle
    property string detail

    StatusCard {
        warning: true
        iconName: "warning"
        title: page.pageTitle.length > 0 ? page.pageTitle + " couldn't open" : "This page couldn't open"
        description: "Something this page needs is missing or broken. Updating Wild OS usually fixes it. If it doesn't, please tell us what you were doing."

        UI.Button {
            text: "Report the problem"
            onClicked: Qt.openUrlExternally("https://help.wildos.io/troubleshooting/report-a-problem/")
        }
    }

    UI.Label {
        Layout.fillWidth: true
        visible: page.detail.length > 0
        text: page.detail
        size: 12
        color: UI.Style.inkMuted
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
    }
}
