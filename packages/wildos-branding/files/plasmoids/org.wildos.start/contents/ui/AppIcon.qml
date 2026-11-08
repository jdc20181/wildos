import WildOS.UI as UI

// Every app and file icon in Start: the icon's own transparent margin is
// measured and cut away, then the visible part is fitted into the same
// share of the box the Start artboard's icons fill (40 of 48), so a theme
// icon drawn with padding and an app's own edge-to-edge PNG come out the
// same height in a grid, list or result row.
UI.ThemeIcon {
    trim: true
    contentScale: 40 / 48
}
