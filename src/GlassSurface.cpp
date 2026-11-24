#include "GlassSurface.h"

#include <QLinearGradient>
#include <QPainter>
#include <QScreen>
#include <QWindow>

#include <algorithm>
#include <cmath>

namespace {
struct CornerRadii {
    qreal left;
    qreal right;
};

int glassAlpha(bool dark, int transparency, BackdropStyle style)
{
    // Popups sit over text in the panel and need more tint to separate the two layers.
    const int lightMinimum = style == BackdropStyle::PopupMenu ? 180 : 146;
    const int minimum = dark ? 200 : lightMinimum;
    return 255 - qRound((255 - minimum) * qBound(0, transparency, 100) / 100.0);
}

CornerRadii cornerRadii(const QSizeF &size, qreal openness, qreal spread, qreal radius)
{
    constexpr qreal handleRadius = 13;
    const qreal maximum = std::min(size.width(), size.height()) / 2;
    return {std::min(handleRadius + (radius - handleRadius) * openness + spread, maximum),
            std::min((radius + spread) * openness, maximum)};
}
} // namespace

QPainterPath glassOutline(const QRectF &bounds, qreal openness, qreal spread, qreal panelRadius)
{
    const auto [left, right] = cornerRadii(bounds.size(), openness, spread, panelRadius);
    const auto tl = bounds.topLeft();
    const auto tr = bounds.topRight();
    const auto bl = bounds.bottomLeft();
    const auto br = bounds.bottomRight();

    QPainterPath path;
    constexpr qreal controlOffset = 0.44771525; // Control-point distance from the bounding-box corner.
    path.moveTo(tl + QPointF(left, 0));
    path.lineTo(tr - QPointF(right, 0));
    path.cubicTo(tr - QPointF(right * controlOffset, 0), tr + QPointF(0, right * controlOffset), tr + QPointF(0, right));
    path.lineTo(br - QPointF(0, right));
    path.cubicTo(br - QPointF(0, right * controlOffset), br - QPointF(right * controlOffset, 0), br - QPointF(right, 0));
    path.lineTo(bl + QPointF(left, 0));
    path.cubicTo(bl + QPointF(left * controlOffset, 0), bl - QPointF(0, left * controlOffset), bl - QPointF(0, left));
    path.lineTo(tl + QPointF(0, left));
    path.cubicTo(tl + QPointF(0, left * controlOffset), tl + QPointF(left * controlOffset, 0), tl + QPointF(left, 0));
    path.closeSubpath();
    return path;
}

QRegion glassCoreRegion(QSize size, qreal openness)
{
    // Windows blur has a binary mask. Admit only fully covered cells and let
    // the per-pixel-alpha window underneath finish the fractional coverage.
    const auto [left, right] = cornerRadii(size, openness, 0, 21);
    const auto inset = [](qreal radius, int y) {
        if (y >= radius)
            return 0;
        return qCeil(radius - std::sqrt(y * (2 * radius - y)));
    };
    const int cornerRows = qCeil(std::max(left, right));
    QRegion region(0, cornerRows, size.width(), size.height() - 2 * cornerRows);
    for (int y = 0; y < cornerRows; ++y) {
        const int x = inset(left, y);
        const int width = size.width() - x - inset(right, y);
        region += QRect(x, y, width, 1);
        region += QRect(x, size.height() - 1 - y, width, 1);
    }
    return region;
}

void paintGlass(QPainter &painter, const QRectF &bounds, qreal openness, bool dark, int transparency,
                qreal radius, BackdropStyle style)
{
    painter.setRenderHint(QPainter::Antialiasing);

    const QPainterPath face = glassOutline(bounds, openness, 0, radius);
    QLinearGradient fill(bounds.topLeft(), bounds.bottomRight());
    const int alpha = glassAlpha(dark, transparency, style);
    const int variation = std::min({5, alpha - 1, 255 - alpha});
    if (dark) {
        fill.setColorAt(0, QColor(49, 54, 62, alpha - variation));
        fill.setColorAt(1, QColor(31, 36, 44, alpha + variation));
    } else if (style == BackdropStyle::PopupMenu) {
        fill.setColorAt(0, QColor(250, 250, 250, alpha + variation));
        fill.setColorAt(1, QColor(241, 241, 241, alpha - variation));
    } else {
        fill.setColorAt(0, QColor(255, 255, 255, alpha + variation));
        fill.setColorAt(1, QColor(252, 252, 252, alpha - variation));
    }
    painter.fillPath(face, fill);

    const qreal hairline = 1.0 / painter.device()->devicePixelRatioF();
    const qreal inset = hairline / 2;
    // At the screen edge, continue the highlight past the visible right side.
    const qreal rightInset = inset - hairline * (1 - openness);
    const auto rim = glassOutline(bounds.adjusted(inset, inset, -rightInset, -inset), openness, -inset, radius);
    QLinearGradient edge(bounds.topLeft(), bounds.bottomRight());
    edge.setColorAt(0, QColor(255, 255, 255, dark ? 32 : 37));
    edge.setColorAt(1, QColor(255, 255, 255, 4));
    painter.strokePath(rim, QPen(QBrush(edge), hairline));
}

GlassShadow::GlassShadow()
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                          | Qt::NoDropShadowWindowHint | Qt::WindowTransparentForInput
                          | Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_QuitOnClose, false);
}

void GlassShadow::setTransparency(int percent)
{
    transparency_ = percent;
    update();
}

void GlassShadow::positionBehind(const QRect &surface, QScreen *screen, qreal openness, bool dark,
                                 const QRegion &core)
{
    const QRect shadowBounds = surface.adjusted(-21, -17, 21, 28).intersected(screen->geometry());
    surface_ = QRectF(surface.translated(-shadowBounds.topLeft()));
    core_ = core.translated(surface_.topLeft().toPoint());
    openness_ = openness;
    dark_ = dark;

    if (!windowHandle())
        winId();
    if (windowHandle()->screen() != screen)
        windowHandle()->setScreen(screen);
    setGeometry(shadowBounds);
    update();
}

void GlassShadow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setClipRegion(QRegion(rect()).subtracted(core_));
    const auto paintLayer = [this, &painter](int radius, int offset, int density) {
        const auto center = surface_.translated(0, offset);
        for (int spread = radius; spread > 0; --spread) {
            const int alpha = qRound(density * (radius - spread + 1.0) / radius);
            if (alpha > 0) {
                const auto ring = center.adjusted(-spread, -spread, spread, spread);
                painter.fillPath(glassOutline(ring, openness_, spread), QColor(18, 26, 38, alpha));
            }
        }
    };
    paintLayer(13, 3, 1);
    paintLayer(21, 7, dark_ ? 3 : 2);

    const auto face = glassOutline(surface_, openness_);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.fillPath(face, Qt::black);

    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setClipRegion(QRegion(surface_.toRect()).subtracted(core_));
    QColor tint = dark_ ? QColor(40, 45, 53) : QColor(254, 254, 254);
    tint.setAlpha(glassAlpha(dark_, transparency_, BackdropStyle::DockedPanel));
    painter.fillPath(face, tint);
}
