#pragma once

#include <QtGui/qwindowdefs.h>
#include <QSize>
#include <QRegion>

enum class BackdropStyle { DockedPanel, PopupMenu };
bool enableWindowBackdrop(WId windowId, BackdropStyle style = BackdropStyle::DockedPanel);
void clipPopupBackdrop(WId windowId, QSize pixels, qreal pixelRadius);
bool windowAnimationsEnabled();
void placeWindowBelow(WId windowId, WId above);
bool panelWindowOnTop(WId windowId);
void restorePanelWindow(WId windowId, const QRegion &mask, qreal scale);
