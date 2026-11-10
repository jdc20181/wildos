#pragma once

class QSize;
class QString;
class QWindow;

// Wayland leaves where a window is to the compositor: a window that changes
// size from its own side grows and shrinks from its top left corner. KWin
// can move and resize it in one step, so a loading window that takes its
// app's size does so about its centre.
namespace KWinPlace
{
// Asks KWin to give the window this size with its centre where it is.
// False when KWin cannot be asked; the window then resizes itself.
bool resizeAroundCentre(QWindow *window, const QString &appId, const QSize &to);
}
