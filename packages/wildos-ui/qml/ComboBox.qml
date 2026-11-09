import QtQuick
import QtQuick.Effects
import QtQuick.Templates as T

// The design's drop-down (Personalisation.dc "Centred", Settings.dc "Ask
// me first"): a 34px button, 0.06 fill with a 0.14 edge, radius 8, the
// choice in 13px and a 12px chevron. From wildos-settings DropDown, rebuilt
// on T.ComboBox so it gains what that one lacked: arrow keys and type to
// choose on the closed control, keyboard focus inside the list, and a list
// that opens upwards when the window has no room below (it used to be cut
// off at the window edge). The list uses the Menu glass and rows; the
// current choice has the Settings accent bar.
//
// Give it `model: [{ value, text }]` (textRole "text", valueRole "value"
// are the defaults), read `currentValue`, and set the choice from the
// backend with `currentIndex: model.findIndex(o => o.value === backend.value)`.
// Not indexOfValue(): a binding doesn't see the list it searches, so it ran
// once before the list was loaded and left the box blank.
T.ComboBox {
    id: control

    property bool previewHover: false
    property bool previewFocus: false
    // "field" is the drop-down inside a form (FirstRegion.dc "Display
    // language"): 42px, the sunken field fill with the 0.16 edge and 14px
    // text, like a large TextField beside it.
    property string look: "button"
    readonly property bool field: look === "field"

    readonly property bool lit: (hovered || previewHover) && enabled
    readonly property real widest: {
        let w = 0;
        const n = delegateModel ? delegateModel.count : 0;
        for (let i = 0; i < n; ++i) {
            w = Math.max(w, metrics.advanceWidth(textAt(i)));
        }
        return w;
    }

    textRole: "text"
    valueRole: "value"
    implicitWidth: Math.max(120, Math.ceil(widest) + leftPadding + rightPadding)
    implicitHeight: Math.max(field ? Style.fieldHeightLarge : 34, implicitContentHeight + 14)
    leftPadding: 12
    rightPadding: 12 + 12 + 10
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    opacity: enabled ? 1 : 0.5
    Accessible.name: displayText

    FontMetrics {
        id: metrics
        font.family: Style.fontFamily
        font.pixelSize: Style.fontBodySm
        font.weight: Font.DemiBold
    }

    background: Rectangle {
        radius: Style.radiusSm
        color: control.field ? Style.surfaceSunken
             : control.down || control.popup.visible ? Style.surfacePressed : control.lit ? Style.surfaceHover : Style.surfaceControl
        border.width: 1
        border.color: control.field ? (control.popup.visible ? Style.accentBorderStrong : Style.borderControl) : Style.borderMenu
        Behavior on color {
            ColorAnimation { duration: Style.hoverDuration }
        }

        FocusRing {
            visible: control.visualFocus || control.previewFocus
            targetRadius: Style.radiusSm
        }
    }

    contentItem: Label {
        text: control.displayText
        size: control.field ? 14 : 13
        color: control.field ? Style.inkField : Style.ink
    }

    indicator: Icon {
        x: control.width - width - 12
        y: (control.height - height) / 2
        name: "chevron-down"
        size: 12
        strokeWidth: 2.2
        rotation: control.popup.visible ? 180 : 0
        Behavior on rotation {
            NumberAnimation { duration: Style.motionFast; easing.type: Style.easeStandard }
        }
    }

    delegate: T.ItemDelegate {
        id: option

        required property int index
        required property var model
        readonly property bool current: control.currentIndex === index

        width: ListView.view ? ListView.view.width : implicitWidth
        implicitHeight: Math.max(Style.menuItemHeight, implicitContentHeight + 12)
        highlighted: control.highlightedIndex === index
        hoverEnabled: true
        padding: 0
        text: model[control.textRole] ?? ""
        Accessible.role: Accessible.ListItem
        Accessible.name: text
        Accessible.selected: current

        background: Rectangle {
            radius: Style.radiusSm
            color: option.down ? Style.surfacePressed : option.highlighted || option.hovered ? Style.surfaceHover : option.current ? Style.surfaceSelected : "transparent"

            Rectangle {
                visible: option.current
                x: 0
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: 16
                radius: 2
                color: Style.accent
            }
        }

        contentItem: Label {
            leftPadding: 14
            text: option.text
            font.weight: option.current ? Font.DemiBold : Font.Normal
        }
    }

    popup: T.Popup {
        id: list

        property bool upwards: false

        // Positions are not bindable, so the room below is measured each
        // time the list opens.
        onAboutToShow: {
            const top = control.mapToItem(null, 0, 0).y;
            const below = control.Window.height - top - control.height;
            upwards = below < implicitHeight + 10 && top > below;
        }

        y: upwards ? -implicitHeight - 4 : control.height + 4
        x: control.width - width
        width: Math.max(control.width, Math.ceil(control.widest) + 14 + 24 + 12)
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 332)
        padding: 6
        margins: 6

        // Long lists, such as the hours of the day, open at the choice.
        onOpened: view.positionViewAtIndex(Math.max(0, control.currentIndex), ListView.Center)

        contentItem: ListView {
            id: view
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            clip: true
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: ScrollBar {}
        }

        background: Rectangle {
            radius: Style.radiusLg
            color: Style.glassMenu
            border.width: 1
            border.color: Style.borderMenu
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Style.shadowMenu
                shadowVerticalOffset: 12
                shadowBlur: 1.0
                blurMax: 32
            }
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Style.motionFast; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Style.motionFast; easing.type: Style.easeExit }
        }
    }
}
