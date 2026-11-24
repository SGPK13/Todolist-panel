#pragma once

#include <QColor>

class GlassMenu;
class QPoint;
class QString;

QColor pickProjectColor(GlassMenu &menu, const QPoint &anchor, const QString &project,
                        const QColor &initial, bool dark);
