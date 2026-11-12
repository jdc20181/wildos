import QtQuick
import QtQuick.Shapes

// The logo's W and diamond as a white stroke, never filled, on a layer of
// its own for OpacityMask to cut the spectrum with. Sizes are in the logo's
// units; `pad` is the room left around the path's bounds.
Item {
    id: outline

    property real unit: 1
    property real pad: 0
    property real lineWidth: 1

    visible: false
    layer.enabled: true
    // Shape's default renderer leaves edges aliased; this one keeps its
    // mitred corners exact, so the layer smooths them instead.
    layer.samples: 4

    Shape {
        x: (outline.pad - 23.66) * outline.unit
        y: (outline.pad - 17) * outline.unit

        ShapePath {
            scale: Qt.size(outline.unit, outline.unit)
            fillColor: "transparent"
            strokeColor: "white"
            strokeWidth: outline.lineWidth * outline.unit
            joinStyle: ShapePath.MiterJoin
            // SVG's default. The W's tips meet at about 60 degrees, which
            // needs a limit of 2; at Qt's default of 2 they could bevel.
            miterLimit: 4

            PathSvg {
                path: "M37 17 L50.34 40 L64.75 64.84 L79.16 40 L92.5 17 L105.84 40 L134.26 89 L120.76 112.28 L92.5 63.55 L64.91 111.12 L23.66 40Z M148.5 17 L162 40 L148.5 63 L135 40Z"
            }
        }
    }
}
