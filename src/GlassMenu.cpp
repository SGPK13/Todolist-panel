#include "GlassMenu.h"
#include "GlassSurface.h"
#include "WindowBackdrop.h"

#include <QPainter>
#include <QProxyStyle>
#include <QStyleOptionMenuItem>
#include <QEvent>
#include <QGraphicsBlurEffect>
#include <QSet>

namespace {
constexpr int radius = 14;

// DWM blurs the desktop, but another layered HWND can still show sharply
// through a popup. Soften just the covered part of our own foreground too.
class MenuUnderlay final : public QGraphicsBlurEffect
{
public:
    explicit MenuUnderlay(QWidget *content) : QGraphicsBlurEffect(content), content_(content)
    {
        setEnabled(false);
        setBlurHints(QGraphicsBlurEffect::QualityHint);
    }

    void track(QMenu *menu)
    {
        menu->installEventFilter(this);
        connect(menu, &QObject::destroyed, this, [this, menu] {
            if (open_.remove(menu))
                refresh();
        });
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        auto *menu = qobject_cast<QMenu *>(object);
        if (!menu)
            return false;
        switch (event->type()) {
        case QEvent::Show:
            open_.insert(menu);
            refresh();
            break;
        case QEvent::Hide:
            open_.remove(menu);
            refresh();
            break;
        case QEvent::Move:
        case QEvent::Resize:
            if (open_.contains(menu))
                update();
            break;
        default:
            break;
        }
        return false;
    }

    void draw(QPainter *painter) override
    {
        const QRegion bounds(content_->rect());
        QRegion covered;
        for (auto *menu : open_) {
            const QRect area(content_->mapFromGlobal(menu->geometry().topLeft()), menu->size());
            covered += QRegion(glassOutline(area, 1, 0, radius).toFillPolygon().toPolygon());
        }
        covered &= bounds;
        if (!covered.isEmpty()) {
            painter->save();
            painter->setClipRegion(covered, Qt::IntersectClip);
            QGraphicsBlurEffect::draw(painter);
            painter->restore();
        }
        painter->save();
        painter->setClipRegion(bounds.subtracted(covered), Qt::IntersectClip);
        drawSource(painter);
        painter->restore();
    }

private:
    void refresh()
    {
        setBlurRadius(14 * content_->devicePixelRatioF());
        setEnabled(!open_.isEmpty());
        update();
    }

    QWidget *content_;
    QSet<QMenu *> open_;
};

class MenuStyle final : public QProxyStyle
{
public:
    explicit MenuStyle(bool dark) : QProxyStyle(QStringLiteral("Fusion")), dark_(dark) {}

    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override
    {
        if (metric == PM_MenuPanelWidth)
            return 0;
        if (metric == PM_MenuHMargin || metric == PM_MenuVMargin)
            return 7;
        return QProxyStyle::pixelMetric(metric, option, widget);
    }

    int styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                  QStyleHintReturn *result) const override
    {
        if (hint == SH_Menu_SupportsSections)
            return 1;
        return QProxyStyle::styleHint(hint, option, widget, result);
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contents,
                           const QWidget *widget) const override
    {
        const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (type != CT_MenuItem || !item)
            return QProxyStyle::sizeFromContents(type, option, contents, widget);
        if (item->menuItemType == QStyleOptionMenuItem::Separator)
            return QSize(1, item->text.isEmpty() ? 9 : 22);
        const int iconSpace = item->icon.isNull() ? 0 : 18;
        return QSize(qBound(116, item->fontMetrics.horizontalAdvance(item->text) + 42 + iconSpace, 246), 31);
    }

    void drawPrimitive(PrimitiveElement pe, const QStyleOption *option, QPainter *p,
                       const QWidget *widget) const override
    {
        if (pe != PE_PanelMenu && pe != PE_FrameMenu)
            QProxyStyle::drawPrimitive(pe, option, p, widget);
    }

    void drawControl(ControlElement control, const QStyleOption *option, QPainter *p,
                     const QWidget *widget) const override
    {
        const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (control != CE_MenuItem || !item) {
            QProxyStyle::drawControl(control, option, p, widget);
            return;
        }
        const QRect row = item->rect;
        QRect label = row.adjusted(10, 0, -24, 0);
        const QColor ink = dark_ ? QColor("#f0f3f7") : QColor("#222a32");
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setFont(item->font);
        if (item->menuItemType == QStyleOptionMenuItem::Separator) {
            if (item->text.isEmpty()) {
                p->setPen(QPen(dark_ ? QColor(255, 255, 255, 36) : QColor(64, 75, 84, 34),
                               1.0 / p->device()->devicePixelRatioF()));
                p->drawLine(row.left() + 9, row.center().y(), row.right() - 9, row.center().y());
            } else {
                QFont font = item->font;
                font.setPixelSize(11);
                font.setWeight(QFont::DemiBold);
                p->setFont(font);
                p->setPen(dark_ ? QColor("#bfcbd4") : QColor("#53626c"));
                p->drawText(label, Qt::AlignVCenter | Qt::AlignLeft, item->text);
            }
        } else {
            if (item->state.testFlag(State_Selected) && item->state.testFlag(State_Enabled)) {
                p->setPen(Qt::NoPen);
                p->setBrush(dark_ ? QColor(232, 242, 255, 27) : QColor(57, 77, 95, 19));
                p->drawRoundedRect(row, 8, 8);
            }
            QColor text = ink;
            if (const auto *menu = qobject_cast<const QMenu *>(widget)) {
                if (const auto *action = menu->actionAt(row.center())) {
                    const auto color = action->property("menuActionTextColor").value<QColor>();
                    if (color.isValid())
                        text = color;
                }
            }
            if (!item->icon.isNull()) {
                item->icon.paint(p, QRect(label.left(), row.center().y() - 8, 16, 16));
                label.setLeft(label.left() + 18);
            }
            if (!item->state.testFlag(State_Enabled))
                text.setAlpha(110);
            p->setPen(text);
            p->drawText(label, Qt::AlignVCenter | Qt::AlignLeft,
                        item->fontMetrics.elidedText(item->text, Qt::ElideRight, label.width()));
            p->translate(row.right() - 14, row.center().y());
            QColor indicator = ink;
            if (!item->state.testFlag(State_Enabled))
                indicator.setAlpha(110);
            p->setPen(QPen(indicator, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPainterPath path;
            if (item->checked) {
                path.moveTo(-4, 0);
                path.lineTo(-1, 3);
                path.lineTo(5, -4);
            } else if (item->menuItemType == QStyleOptionMenuItem::SubMenu) {
                path.moveTo(-2, -4);
                path.lineTo(2, 0);
                path.lineTo(-2, 4);
            }
            p->setBrush(Qt::NoBrush);
            p->drawPath(path);
        }
        p->restore();
    }

private:
    bool dark_;
};
} // namespace

GlassMenu::GlassMenu(QWidget *parent, QWidget *panelContent, bool dark, int transparency)
    : QMenu(parent), dark_(dark), transparency_(transparency)
{
    auto *underlay = dynamic_cast<MenuUnderlay *>(panelContent->graphicsEffect());
    if (!underlay) {
        underlay = new MenuUnderlay(panelContent);
        panelContent->setGraphicsEffect(underlay);
    }
    underlay->track(this);
    setWindowFlag(Qt::FramelessWindowHint);
    setWindowFlag(Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setSeparatorsCollapsible(false);
    QFont menuFont = font();
    menuFont.setPixelSize(14);
    setFont(menuFont);
    auto *menuStyle = new MenuStyle(dark);
    menuStyle->setParent(this);
    setStyle(menuStyle);
}

void GlassMenu::setTransparency(int percent)
{
    transparency_ = percent;
    update();
}

void GlassMenu::setActionTextColor(QAction *action, const QColor &color)
{
    action->setProperty("menuActionTextColor", color);
}

void GlassMenu::clipBackdrop()
{
    const qreal scale = devicePixelRatioF();
    clipPopupBackdrop(winId(), size() * scale, radius * scale);
}

void GlassMenu::showEvent(QShowEvent *event)
{
    QMenu::showEvent(event);
    clipBackdrop();
    blurred_ = enableWindowBackdrop(winId(), BackdropStyle::PopupMenu);
}

bool GlassMenu::event(QEvent *event)
{
    const bool result = QMenu::event(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if (event->type() == QEvent::DevicePixelRatioChange && isVisible())
        clipBackdrop();
#endif
    return result;
}

void GlassMenu::resizeEvent(QResizeEvent *event)
{
    QMenu::resizeEvent(event);
    if (isVisible())
        clipBackdrop();
}

void GlassMenu::paintEvent(QPaintEvent *event)
{
    {
        QPainter painter(this);
        paintGlass(painter, QRectF(rect()), 1, dark_, blurred_ ? transparency_ : 0,
                   radius, BackdropStyle::PopupMenu);
    }
    QMenu::paintEvent(event);
}
