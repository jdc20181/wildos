import QtQuick
import QtQuick.Templates as T

// A button that is only an icon: navigation and command bar buttons in
// Files (36 by 34, radius 8, FilesDetails.dc), Back in Settings (34 by 30,
// radius 6), tray and flyout buttons. Based on wildos-files IconButton,
// whose hover label stays away once clicked so it never covers the menu
// the button opened. Every icon button needs a `tip`: it is the tooltip and
// the accessible name.
//
// `selected` fills the button with the accent, for a tool picked from a row
// (the screenshot toolbar's capture modes, Screenshot.dc). `glyph` and
// `dashedGlyph` draw raw path data instead of a named icon.
T.Button {
    id: control

    property string iconName
    property real iconSize: 16
    property real stroke: iconSize <= 16 ? 2 : 1.8
    property color iconColor: Style.iconInk
    property string tip
    property real radius: Style.radiusSm
    property bool previewHover: false
    property bool previewFocus: false
    property bool tipDismissed: false
    property bool selected: false
    property string glyph
    property string dashedGlyph

    readonly property bool lit: (hovered || previewHover) && enabled

    implicitWidth: 36
    implicitHeight: 34
    padding: 0
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.name: tip
    Accessible.role: Accessible.Button

    onClicked: tipDismissed = true
    onHoveredChanged: if (!hovered) tipDismissed = false

    background: Rectangle {
        radius: control.radius
        color: control.selected ? (control.down ? Style.accentPressed : control.lit ? Style.accentHover : Style.accent)
            : control.down ? Style.surfacePressed : control.lit || control.checked ? Style.surfaceHover : "transparent"
        Behavior on color {
            ColorAnimation { duration: Style.hoverDuration }
        }

        FocusRing {
            visible: control.visualFocus || control.previewFocus
            targetRadius: control.radius
        }
    }

    contentItem: Item {
        Icon {
            visible: control.glyph.length === 0 && control.dashedGlyph.length === 0
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            strokeWidth: control.stroke
            color: !control.enabled ? Style.inkDisabled : control.selected ? Style.textOnAccent : control.iconColor
        }

        Glyph {
            visible: control.glyph.length > 0 || control.dashedGlyph.length > 0
            anchors.centerIn: parent
            width: control.iconSize
            height: control.iconSize
            path: control.glyph
            dashedPath: control.dashedGlyph
            strokeWidth: control.stroke
            color: !control.enabled ? Style.inkDisabled : control.selected ? Style.textOnAccent : control.iconColor
        }
    }

    Tooltip {
        text: control.tip
        visible: control.hovered && !control.tipDismissed && control.tip.length > 0
    }
}
