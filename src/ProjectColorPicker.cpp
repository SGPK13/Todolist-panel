#include "ProjectColorPicker.h"
#include "GlassMenu.h"

#include <QColorDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <functional>

namespace {
class ColorField final : public QWidget
{
public:
    explicit ColorField(QWidget *parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("projectColorField"));
        setFixedHeight(100);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::CrossCursor);
        setAccessibleName(QStringLiteral("饱和度和明度，使用方向键调整"));
    }

    int hue = 0;
    std::function<void(QColor)> edited;

    void setColor(const QColor &color)
    {
        if (color.hsvHue() >= 0)
            hue = color.hsvHue();
        saturation_ = color.hsvSaturation();
        value_ = color.value();
        update();
    }

    QColor color() const { return QColor::fromHsv(hue, saturation_, value_); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF area = bounds();
        QPainterPath outline;
        outline.addRoundedRect(area, 9, 9);
        QLinearGradient chroma(area.topLeft(), area.topRight());
        chroma.setColorAt(0, Qt::white);
        chroma.setColorAt(1, QColor::fromHsv(hue, 255, 255));
        p.fillPath(outline, chroma);
        QLinearGradient shade(area.topLeft(), area.bottomLeft());
        shade.setColorAt(0, QColor(0, 0, 0, 0));
        shade.setColorAt(1, Qt::black);
        p.fillPath(outline, shade);
        p.setPen(QPen(QColor(110, 118, 124, 70), 1));
        p.drawPath(outline);
        const QPointF point(area.left() + saturation_ * area.width() / 255,
                            area.bottom() - value_ * area.height() / 255);
        p.setPen(QPen(QColor(0, 0, 0, 135), 3.5));
        p.drawEllipse(point, 4.5, 4.5);
        p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(point, 4.5, 4.5);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            select(event->position());
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (event->buttons().testFlag(Qt::LeftButton))
            select(event->position());
    }
    void keyPressEvent(QKeyEvent *event) override
    {
        const int step = event->modifiers().testFlag(Qt::ShiftModifier) ? 10 : 1;
        switch (event->key()) {
        case Qt::Key_Left:  saturation_ = qMax(0, saturation_ - step); break;
        case Qt::Key_Right: saturation_ = qMin(255, saturation_ + step); break;
        case Qt::Key_Down:  value_ = qMax(0, value_ - step); break;
        case Qt::Key_Up:    value_ = qMin(255, value_ + step); break;
        default: QWidget::keyPressEvent(event); return;
        }
        update();
        edited(color());
    }

private:
    QRectF bounds() const { return QRectF(rect()).adjusted(6, 6, -6, -6); }
    void select(QPointF point)
    {
        const auto area = bounds();
        saturation_ = qBound(0, qRound(255 * (point.x() - area.left()) / area.width()), 255);
        value_ = qBound(0, qRound(255 * (area.bottom() - point.y()) / area.height()), 255);
        update();
        edited(color());
    }
    int saturation_ = 0;
    int value_ = 0;
};
}

QColor pickProjectColor(GlassMenu &menu, const QPoint &anchor, const QString &project,
                        const QColor &initial, bool dark)
{
    auto *entry = new QWidgetAction(&menu);
    auto *card = new QWidget;
    entry->setDefaultWidget(card);
    menu.addAction(entry);
    card->setFixedWidth(224);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(7, 5, 7, 6);
    layout->setSpacing(5);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("项目颜色"));
    title->setStyleSheet(QStringLiteral("font-size: 12px;"));
    header->addWidget(title);
    header->addStretch();
    auto *more = new QPushButton(QStringLiteral("···"));
    more->setFixedSize(25, 22);
    more->setStyleSheet(QStringLiteral("QPushButton {color:%1;font-size:15px;}")
                            .arg(dark ? "#c9d3dd" : "#475965"));
    more->setAccessibleName(QStringLiteral("更多调色工具"));
    more->setToolTip(QStringLiteral("更多调色工具 · 屏幕取色、RGB 和收藏色"));
    header->addWidget(more);
    layout->addLayout(header);
    auto *preview = new QLabel;
    preview->setTextFormat(Qt::PlainText);
    preview->setToolTip(project);
    layout->addWidget(preview);
    auto *field = new ColorField(card);
    layout->addWidget(field);
    auto *hue = new QSlider(Qt::Horizontal);
    hue->setAccessibleName(QStringLiteral("色相"));
    hue->setRange(0, 359);
    hue->setFixedHeight(18);
    hue->setCursor(Qt::PointingHandCursor);
    hue->setStyleSheet(QStringLiteral(
        "QSlider { margin:0 5px; } QSlider::groove:horizontal {height:7px;border-radius:3px;"
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #db625f,stop:0.1667 #dddb6a,"
        "stop:0.3333 #76cc86,stop:0.5 #65cccc,stop:0.6667 #797ade,stop:0.8333 #d574d4,stop:1 #db625f);}"
        "QSlider::handle:horizontal {width:10px;margin:-3px 0;border-radius:6px;"
        "background:#fafafa;border:1px solid #62707b;}"));
    layout->addWidget(hue);
    auto *footer = new QHBoxLayout;
    auto *hex = new QLineEdit;
    hex->setObjectName(QStringLiteral("projectHex"));
    hex->setAccessibleName(QStringLiteral("十六进制色值"));
    hex->setPlaceholderText(QStringLiteral("#RRGGBB"));
    hex->setFixedHeight(29);
    hex->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("#?[0-9a-fA-F]{6}")), hex));
    hex->setStyleSheet(QStringLiteral("QLineEdit {background:%1;border:1px solid %2;border-radius:7px;"
        "padding:0 8px;font-family:Consolas;font-size:13px;} QLineEdit:focus {border-color:#739bac;}")
        .arg(dark ? "#343b43" : "#f0f2f4", dark ? "#535e69" : "#d6dde1"));
    footer->addWidget(hex, 1);
    auto *apply = new QPushButton(QStringLiteral("应用"));
    apply->setObjectName(QStringLiteral("applyProjectColor"));
    apply->setFixedSize(56, 29);
    apply->setStyleSheet(QStringLiteral("QPushButton {background:%1;color:%2;border-radius:7px;font-size:13px;}"
        "QPushButton:disabled {background:#899398;color:#eceff0;}")
        .arg(dark ? "#c4d9da" : "#385864", dark ? "#192c32" : "#ffffff"));
    footer->addWidget(apply);
    layout->addLayout(footer);

    QColor selected;
    const auto previewColor = [&](QColor color) {
        selected = color;
        preview->setStyleSheet(QStringLiteral("font-size:17px;font-weight:600;color:%1;").arg(color.name()));
        preview->setText(preview->fontMetrics().elidedText(project, Qt::ElideRight, 204));
    };
    const auto syncControls = [&](QColor color) {
        previewColor(color);
        hex->setText(color.name().toUpper());
        apply->setEnabled(true);
    };
    const auto setColor = [&](QColor color) {
        field->setColor(color);
        const QSignalBlocker block(hue);
        hue->setValue(field->hue);
        syncControls(color);
    };
    setColor(initial);
    field->edited = syncControls;
    QObject::connect(hue, &QSlider::valueChanged, card, [&](int angle) {
        field->hue = angle;
        field->update();
        syncControls(field->color());
    });
    QObject::connect(hex, &QLineEdit::textEdited, card, [&](QString text) {
        apply->setEnabled(hex->hasAcceptableInput());
        if (!hex->hasAcceptableInput())
            return;
        const QColor color(text.startsWith('#') ? text : QStringLiteral("#") + text);
        field->setColor(color);
        const QSignalBlocker block(hue);
        hue->setValue(field->hue);
        previewColor(color);
    });
    bool accepted = false;
    bool advanced = false;
    QObject::connect(apply, &QPushButton::clicked, card, [&] { accepted = true; menu.close(); });
    QObject::connect(hex, &QLineEdit::returnPressed, apply, &QPushButton::click);
    QObject::connect(more, &QPushButton::clicked, card, [&] { advanced = true; menu.close(); });
    do {
        advanced = false;
        menu.exec(anchor);
        if (advanced) {
            const QColor picked = QColorDialog::getColor(selected, menu.parentWidget(), QStringLiteral("更多调色工具"),
                                                          QColorDialog::DontUseNativeDialog);
            if (picked.isValid())
                setColor(picked);
        }
    } while (advanced);
    return accepted ? selected : QColor();
}
