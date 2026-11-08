import QtQuick
import QtQuick.Effects
import WildOS.UI as UI

// Dark translucent glass. When a backdrop item is given, the part of it behind
// this surface is blurred first, which is what makes the tint read as glass
// rather than a flat grey box. Software rendering cannot run the blur, so the
// tint is made a little stronger there to keep text legible.
Item {
    id: glass

    property Item backdrop: null
    property color tint: UI.Style.glassOnWallpaperField
    property color borderColor: UI.Style.borderOnWallpaper
    property real radius: height / 2
    property real blurRadius: 30
    property bool shadow: false

    readonly property bool blurAvailable: backdrop !== null
                                          && GraphicsInfo.api !== GraphicsInfo.Software
                                          && GraphicsInfo.api !== GraphicsInfo.Unknown

    // Reading every ancestor's position makes this binding re-run whenever the
    // surface moves on screen, not only when its own x or y change.
    readonly property rect sampleRect: {
        if (!backdrop)
            return Qt.rect(0, 0, 0, 0);
        let moved = 0;
        for (let p = glass; p; p = p.parent)
            moved += p.x + p.y + p.width;
        const origin = glass.mapToItem(backdrop, 0, 0);
        return Qt.rect(origin.x, origin.y, glass.width, glass.height + moved * 0);
    }

    Rectangle {
        id: shadowShape
        visible: false
        anchors.fill: parent
        radius: glass.radius
        color: UI.Style.shadowFlyout
        layer.enabled: glass.shadow && glass.blurAvailable
    }

    MultiEffect {
        visible: glass.shadow && glass.blurAvailable
        anchors.fill: parent
        anchors.topMargin: 24
        source: shadowShape
        blurEnabled: true
        blur: 1.0
        blurMax: 48
        autoPaddingEnabled: true
    }

    ShaderEffectSource {
        id: sample
        visible: false
        anchors.fill: parent
        sourceItem: glass.blurAvailable ? glass.backdrop : null
        sourceRect: glass.sampleRect
        live: true
    }

    Rectangle {
        id: mask
        visible: false
        anchors.fill: parent
        radius: glass.radius
        layer.enabled: glass.blurAvailable
    }

    MultiEffect {
        visible: glass.blurAvailable
        anchors.fill: parent
        source: sample
        blurEnabled: true
        blur: 1.0
        blurMax: Math.round(glass.blurRadius)
        saturation: 0.35
        autoPaddingEnabled: false
        maskEnabled: true
        maskSource: mask
    }

    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        color: glass.blurAvailable ? glass.tint : Qt.rgba(glass.tint.r, glass.tint.g, glass.tint.b, Math.min(1, glass.tint.a + 0.2))
        border.width: 1
        border.color: glass.borderColor
    }
}
