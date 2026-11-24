#pragma once

#include "WindowBackdrop.h"
#include <QPainterPath>
#include <QWidget>

class QPainter;
class QScreen;

QPainterPath glassOutline(const QRectF &bounds, qreal openness, qreal spread = 0, qreal radius = 21);
QRegion glassCoreRegion(QSize size, qreal openness);
void paintGlass(QPainter &painter, const QRectF &bounds, qreal openness, bool dark, int transparency,
                qreal radius = 21, BackdropStyle style = BackdropStyle::DockedPanel);

class GlassShadow final : public QWidget
{
public:
    GlassShadow();
    void setTransparency(int percent);
    void positionBehind(const QRect &surface, QScreen *screen, qreal openness, bool dark,
                        const QRegion &core);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF surface_;
    QRegion core_;
    qreal openness_ = 1;
    bool dark_ = false;
    int transparency_ = 51;
};
