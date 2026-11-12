import QtQuick
import QtQuick.Shapes

// The Wild OS mark on its tile, 64px, as the recovery boards draw it.
Item {
    implicitWidth: 64
    implicitHeight: 64

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1.3
        radius: 15
        color: "#1b1d22"
    }

    Shape {
        // The mark's own box is 150 wide from x 18, y -10.5; it fills the
        // middle 74% of the tile.
        x: 64 * 0.13
        y: 64 * 0.13
        width: 64 * 0.74
        height: 64 * 0.74
        preferredRendererType: Shape.CurveRenderer
        transform: [
            Translate { x: -18; y: 10.5 },
            Scale { xScale: 64 * 0.74 / 150; yScale: 64 * 0.74 / 150 }
        ]

        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 0; y1: 17; x2: 0; y2: 113
                GradientStop { position: 0; color: "#b4caff" }
                GradientStop { position: 0.5; color: "#7d97ec" }
                GradientStop { position: 1; color: "#f0a3ae" }
            }
            PathSvg { path: "M37 17 L50.34 40 L64.75 64.84 L79.16 40 L92.5 17 L105.84 40 L134.26 89 L120.76 112.28 L92.5 63.55 L64.91 111.12 L23.66 40Z M148.5 17 L162 40 L148.5 63 L135 40Z" }
        }
    }
}
