import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import WildOS.UI as UI

// The 1px dividers in the design's taskbar: 28 px tall after Start in the
// apps pill, 24 px tall after the security chip in the status pill. The
// panel already puts 4 px between widgets, so sideMargin is what the design
// adds on top of that.
PlasmoidItem {
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    preferredRepresentation: fullRepresentation

    readonly property int sideMargin: Plasmoid.configuration.sideMargin
    Layout.minimumWidth: 1 + sideMargin * 2
    Layout.maximumWidth: 1 + sideMargin * 2

    fullRepresentation: Item {
        Rectangle {
            anchors.centerIn: parent
            width: 1
            height: Plasmoid.configuration.lineHeight
            color: Plasmoid.configuration.lineHeight > 24 ? UI.Style.borderControl : UI.Style.borderGlass
        }
    }
}
