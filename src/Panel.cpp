#include "Panel.h"
#include "TaskLabels.h"
#include "GlassSurface.h"
#include "GlassMenu.h"
#include "WindowBackdrop.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QFont>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QHelpEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QPushButton>
#include <QScreen>
#include <QSet>
#include <QScopedValueRollback>
#include <QSettings>
#include <QShowEvent>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSystemTrayIcon>
#include <QStyleHints>
#include <QStyledItemDelegate>
#include <QStyleOption>
#include <QStylePainter>
#include <QTextLayout>
#include <QTimer>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <QWidgetAction>

#include <algorithm>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#include <wtsapi32.h>
#endif

namespace {

constexpr int panelWidth = 288;
constexpr int panelPadding = 14;
constexpr int headerHeight = 32;
constexpr int headerGap = 10;
constexpr int composerHeight = 38;
constexpr int compactFooterHeight = 22;
constexpr int compactFooterGap = 2;
constexpr int emptyListHeight = 40;
constexpr int handleWidth = 30;
constexpr int handleHeight = 82;
constexpr int panelMargin = 17;
constexpr int transitionDuration = 315;
constexpr int maxTaskLength = 240;
constexpr int savedTitleRole = Qt::UserRole;

const char *const lightPanelStyle = R"(
    QLabel { color: #222a32; }
    QLabel#title { font-size: 20px; font-weight: 700; }
    QLabel#summary, QLabel#handleCount {
        color: #18212a;
        background: rgba(80, 101, 120, 27);
        border-radius: 10px;
        font-size: 11px;
        font-weight: 600;
        padding: 0 3px;
    }
    QLabel#summary { border-radius: 14px; font-size: 16px; font-weight: 500; }
    QLabel#handleCount { border-radius: 9px; }
    QLabel#empty { color: #303a44; font-size: 14px; padding: 0 10px; }
    QLabel#saveError { color: #651926; font-size: 11px; }
    QPushButton {
        color: #283848;
        background: transparent;
        border: 1px solid transparent;
        border-radius: 8px;
        padding: 0;
    }
    QPushButton:hover { background: rgba(57, 77, 95, 19); }
    QPushButton:focus { border-color: rgba(55, 88, 115, 153); }
    QPushButton#handle {
        background: transparent;
        border: none;
    }
    QWidget#composer { color: #446675; }
    QLineEdit#taskInput { font-size: 15px; padding: 0 7px; border-radius: 7px; placeholder-text-color: #374651; }
    QLineEdit#taskInput:focus { background: #f3f5f5; }
    QLineEdit#newProjectName {
        font-size: 13px; padding: 0 8px; border-radius: 7px; border: 1px solid transparent;
        background: rgba(80, 101, 120, 17); color: #222a32; placeholder-text-color: #374651;
    }
    QLineEdit#newProjectName:focus { background: #f3f5f5; border-color: rgba(55, 88, 115, 90); }
    QPushButton#add {
        background: #466b73;
        border: 1px solid transparent;
        border-radius: 11px;
    }
    QPushButton#add:hover { background: #345761; }
    QPushButton#add:focus { border-color: #233e48; }
    QPushButton#add:disabled { background: rgba(70, 90, 101, 13); border-color: rgba(68, 91, 101, 32); }
    QPushButton#projectFilter { border-radius: 9px; font-size: 12px; }
    QPushButton#projectFilter:checked { background: rgba(78, 118, 150, 24); }
    QPushButton#projectFilter:checked:hover { background: rgba(78, 118, 150, 39); }
    QPushButton#sectionAdd, QPushButton#sectionToggle { background: transparent; border: 0; color: #344d5b; }
    QPushButton#undoTask { font-size: 12px; padding: 0 6px; }
    QLabel#draftTarget { font-size: 12px; padding-left: 5px; }
    QLineEdit {
        color: #222a32;
        placeholder-text-color: #35404a;
        background: transparent;
        border: 0;
        font-size: 16px;
        padding: 4px 6px 4px 1px;
        selection-background-color: #2f648f;
        selection-color: white;
    }
    QListWidget {
        color: #191b1f;
        font-size: 14px;
        background: transparent;
        border: 0;
        outline: 0;
    }
    QListWidget::item {
        padding: 0 7px;
        background: transparent;
        border: 1px solid transparent;
        border-radius: 8px;
    }
    QListWidget::item:hover { background: rgba(255, 255, 255, 50); }
    QListWidget::item:selected {
        color: #222a32;
        background: rgba(198, 220, 235, 85);
    }
    QListWidget QLineEdit {
        background: #f3f6fa;
        border: 1px solid #7398b9;
        padding: 2px 4px;
        border-radius: 4px;
    }
    QScrollBar:vertical { width: 6px; background: transparent; margin: 0; }
    QScrollBar::handle:vertical { min-height: 20px; border-radius: 3px; background: #839bb1; }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
)";

const char *const darkPanelStyle = R"(
    QLabel { color: #f0f3f7; }
    QLabel#summary, QLabel#handleCount {
        color: #f1f4f8;
        background: rgba(18, 29, 40, 73);
    }
    QLabel#empty { color: #e7edf2; }
    QPushButton#projectFilter { color: #e7edf2; }
    QPushButton#projectFilter:checked { background: rgba(137, 183, 218, 28); }
    QPushButton#projectFilter:checked:hover { background: rgba(137, 183, 218, 43); }
    QPushButton#sectionAdd, QPushButton#sectionToggle { color: #cedee8; }
    QLabel#saveError { color: #ffc6cc; }
    QPushButton:hover { background: rgba(232, 242, 255, 27); }
    QPushButton:focus { border-color: rgba(208, 228, 246, 145); }
    QWidget#composer { color: #c1d6db; }
    QLineEdit#taskInput { placeholder-text-color: #c5cfd4; }
    QLineEdit#taskInput:focus { background: #303941; }
    QLineEdit#newProjectName {
        background: rgba(232, 240, 248, 17); color: #f0f3f7; placeholder-text-color: #c5cfd4;
    }
    QLineEdit#newProjectName:focus { background: #303941; border-color: rgba(208, 228, 246, 90); }
    QPushButton#add { background: #bad5d6; }
    QPushButton#add:hover { background: #d4e6e6; }
    QPushButton#add:focus { border-color: #f3fafa; }
    QPushButton#add:disabled { background: rgba(224, 237, 239, 15); border-color: rgba(227, 239, 242, 44); }
    QLineEdit { color: #f0f3f7; placeholder-text-color: #c3cbd5; }
    QListWidget, QListWidget::item:selected { color: #f0f3f7; }
    QListWidget::item:hover { background: rgba(233, 242, 255, 21); }
    QListWidget::item:selected { background: rgba(38, 61, 80, 102); }
    QListWidget QLineEdit { background: #303a45; border-color: #8ea6be; }
    QScrollBar::handle:vertical { background: #899bac; }
)";

enum class Icon { Collapse, Expand, Add, QuickAdd, Filter };

class ProjectFilterButton final : public QPushButton
{
public:
    using QPushButton::QPushButton;

    QSize sizeHint() const override
    {
        return QSize(fontMetrics().horizontalAdvance(text()) + iconSize().width() + labelGap + 2 * sidePadding,
                     minimumHeight());
    }

    int heightForWidth(int width) const override
    {
        QTextLayout label;
        return qMax(minimumHeight(), qCeil(layoutLabel(label, width).height()) + 8);
    }

protected:
    // The selected project, rather than clicking the button, controls the tint.
    void nextCheckState() override {}

    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionButton option;
        initStyleOption(&option);
        option.text.clear();
        option.icon = QIcon();
        QStylePainter painter(this);
        painter.drawControl(QStyle::CE_PushButton, option);
        QTextLayout label;
        const QSizeF textSize = layoutLabel(label, width());
        const qreal left = (width() - iconSize().width() - labelGap - textSize.width()) / 2;
        const QRect iconRect(qRound(left), (height() - iconSize().height()) / 2,
                             iconSize().width(), iconSize().height());
        icon().paint(&painter, iconRect);
        painter.setPen(option.palette.color(QPalette::ButtonText));
        label.draw(&painter, QPointF(left + iconSize().width() + labelGap, (height() - textSize.height()) / 2));
    }

private:
    static constexpr int sidePadding = 3;
    static constexpr int labelGap = 4;

    QSizeF layoutLabel(QTextLayout &label, int width) const
    {
        label.setText(text());
        label.setFont(font());
        QTextOption wrap;
        wrap.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        label.setTextOption(wrap);
        const int available = qMax(1, width - iconSize().width() - labelGap - 2 * sidePadding);
        QSizeF size(0, 0);
        label.beginLayout();
        for (QTextLine line = label.createLine(); line.isValid(); line = label.createLine()) {
            line.setLineWidth(available);
            line.setPosition(QPointF(0, size.height()));
            size.setWidth(qMax(size.width(), line.naturalTextWidth()));
            size.rheight() += line.height();
        }
        label.endLayout();
        return size;
    }
};

class TaskInputStyle final : public QProxyStyle
{
public:
    TaskInputStyle() : QProxyStyle(QStringLiteral("Fusion")) {}

    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override
    {
        const int value = QProxyStyle::pixelMetric(metric, option, widget);
        return metric == PM_TextCursorWidth ? qMax(2, value) : value;
    }
};

class TaskInput final : public QLineEdit
{
public:
    explicit TaskInput(QWidget *parent) : QLineEdit(parent)
    {
        auto *inputStyle = new TaskInputStyle;
        inputStyle->setParent(this);
        setStyle(inputStyle);
        setCursor(Qt::IBeamCursor);
    }

    bool continueEntry() const { return continueEntry_; }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        // Let QLineEdit and the IME decide when Return actually submits.
        const bool enter = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
        QScopedValueRollback<bool> keep(continueEntry_, enter && event->modifiers().testFlag(Qt::ShiftModifier));
        QLineEdit::keyPressEvent(event);
    }

private:
    bool continueEntry_ = false;
};

qreal smoothProgress(qreal value)
{
    const qreal t = qBound(0.0, value, 1.0);
    return t * t * (3 - 2 * t);
}

class TaskComposer final : public QWidget
{
public:
    explicit TaskComposer(QWidget *parent) : QWidget(parent)
    {
        connect(qApp, &QApplication::focusChanged, this, [this](QWidget *before, QWidget *after) {
            if (isAncestorOf(before) != isAncestorOf(after))
                update();
        });
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const bool focused = isAncestorOf(QApplication::focusWidget());
        QColor ink = palette().color(QPalette::WindowText);
        ink.setAlpha(focused ? 119 : 54);
        QColor clear = ink;
        clear.setAlpha(0);
        QLinearGradient rule(0, 0, width(), 0);
        rule.setColorAt(0, clear);
        rule.setColorAt(0.06, ink);
        rule.setColorAt(0.90, ink);
        rule.setColorAt(1, clear);
        QPainter painter(this);
        painter.fillRect(QRectF(0, 0, width(), 1.0 / devicePixelRatioF()), rule);
    }
};

class EdgeHandle final : public QPushButton
{
public:
    using QPushButton::QPushButton;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionButton option;
        initStyleOption(&option);
        const bool keyboardFocus = option.state.testFlag(QStyle::State_HasFocus)
            && option.state.testFlag(QStyle::State_KeyboardFocusChange);
        if (!underMouse() && !isDown() && !keyboardFocus)
            return;

        // The glass supplies the silhouette. Keep feedback around the arrow.
        const int side = 20;
        const QRectF indicator((width() - side) / 2.0, childrenRect().top() - 2, side, side);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(keyboardFocus ? QPen(palette().color(QPalette::Highlight), 1) : QPen(Qt::NoPen));
        painter.setBrush(QColor(128, 128, 128, isDown() ? 64 : 33));
        painter.drawRoundedRect(indicator, 6, 6);
    }
};

QIcon controlIcon(Icon icon, bool dark)
{
    QPainterPath path;
    switch (icon) {
    case Icon::Collapse:
        path.moveTo(6.5, 4);
        path.lineTo(10, 8);
        path.lineTo(6.5, 12);
        break;
    case Icon::Expand:
        path.moveTo(9.5, 4);
        path.lineTo(6, 8);
        path.lineTo(9.5, 12);
        break;
    case Icon::Filter:
        path.moveTo(2.5, 4.5);
        path.lineTo(13.5, 4.5);
        path.moveTo(4.5, 8);
        path.lineTo(11.5, 8);
        path.moveTo(6.5, 11.5);
        path.lineTo(9.5, 11.5);
        break;
    case Icon::QuickAdd:
    case Icon::Add:
        path.moveTo(3.5, 8);
        path.lineTo(12.5, 8);
        path.moveTo(8, 3.5);
        path.lineTo(8, 12.5);
        break;
    }

    QIcon result;
    for (const auto mode : {QIcon::Normal, QIcon::Disabled}) {
        QPixmap pixmap(64, 64);
        pixmap.setDevicePixelRatio(2);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        QColor foreground = dark
            ? QColor(231, 237, 245, mode == QIcon::Disabled ? 167 : 242)
            : QColor(45, 61, 77, mode == QIcon::Disabled ? 157 : 224);
        if (icon == Icon::Add && mode == QIcon::Normal)
            foreground = dark ? QColor("#263f45") : QColor("#ffffff");
        painter.scale(2, 2);
        painter.setPen(QPen(foreground, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        painter.end();
        result.addPixmap(pixmap, mode);
    }
    return result;
}

class TaskListStyle : public QProxyStyle
{
public:
    TaskListStyle() : QProxyStyle(QStringLiteral("Fusion")) {}

    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override
    {
        if (metric == PM_IndicatorWidth || metric == PM_IndicatorHeight)
            return 18;
        return QProxyStyle::pixelMetric(metric, option, widget);
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget) const override
    {
        if (element != PE_IndicatorItemViewItemCheck) {
            QProxyStyle::drawPrimitive(element, option, painter, widget);
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF box = QRectF(option->rect).adjusted(0.8, 0.8, -0.8, -0.8);
        const bool checked = option->state.testFlag(State_On);
        const bool dark = option->palette.color(QPalette::Text).lightness() > 128;
        painter->setPen(checked ? QPen(Qt::NoPen)
                               : QPen(dark ? QColor(226, 233, 240, 154) : QColor(43, 44, 45, 209), 1));
        painter->setBrush(checked ? (dark ? QColor("#bac6cf") : QColor("#576773"))
                                 : QColor(255, 255, 255, dark ? 12 : 51));
        painter->drawRoundedRect(box, 4.5, 4.5);
        if (checked) {
            QPainterPath check;
            const QPointF center = box.center();
            check.moveTo(center + QPointF(-5.1, 0));
            check.lineTo(center + QPointF(-1.4, 3.4));
            check.lineTo(center + QPointF(5.1, -4));
            painter->setPen(QPen(dark ? QColor("#26333d") : QColor(Qt::white),
                                 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(check);
        }
        painter->restore();
    }
};

QRect priorityTarget(const QRect &row)
{
    QRect target(0, 0, 20, 24);
    target.moveCenter(QPoint(row.right() - 10, row.center().y()));
    return target;
}

class SectionAddButton final : public QPushButton
{
protected:
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionButton option;
        initStyleOption(&option);
        const bool keyboardFocus = hasFocus() && option.state.testFlag(QStyle::State_KeyboardFocusChange);
        if (!underMouse() && !parentWidget()->underMouse() && !keyboardFocus && !isDown())
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QPointF center(width() - 10, height() / 2.0);
        if (keyboardFocus) {
            painter.setPen(QPen(option.palette.color(QPalette::Highlight), 1));
            painter.drawRoundedRect(QRectF(center - QPointF(8, 8), QSizeF(16, 16)), 4, 4);
        }
        painter.setPen(QPen(option.palette.color(QPalette::ButtonText), 1.3, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(center - QPointF(3.5, 0), center + QPointF(3.5, 0));
        painter.drawLine(center - QPointF(0, 3.5), center + QPointF(0, 3.5));
    }
};

class SectionToggleButton final : public QPushButton
{
public:
    SectionToggleButton()
    {
        setObjectName(QStringLiteral("sectionToggle"));
        setCheckable(true);
        setFocusPolicy(Qt::TabFocus);
        setCursor(Qt::PointingHandCursor);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addStretch();
        add->setFixedWidth(22);
        add->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        layout->addWidget(add);
    }
    SectionAddButton *const add = new SectionAddButton;

protected:
    void enterEvent(QEnterEvent *event) override { QPushButton::enterEvent(event); add->update(); }
    void leaveEvent(QEvent *event) override { QPushButton::leaveEvent(event); add->update(); }
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionButton option;
        initStyleOption(&option);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(8, height() / 2.0);
        if (hasFocus() && option.state.testFlag(QStyle::State_KeyboardFocusChange)) {
            painter.setPen(QPen(option.palette.color(QPalette::Highlight), 1));
            painter.drawRoundedRect(QRectF(-7,-7,14,14), 3, 3);
        }
        painter.setPen(QPen(option.palette.color(QPalette::ButtonText), 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (!isChecked())
            painter.rotate(90);
        const QPointF chevron[] = {{-1.5, -3}, {1.5, 0}, {-1.5, 3}};
        painter.drawPolyline(chevron, 3);
    }
};

class TaskDelegate final : public QStyledItemDelegate
{
public:
    explicit TaskDelegate(QListWidget *view) : QStyledItemDelegate(view), view_(view) {}

    int rowHeight(const QModelIndex &index, int width) const
    {
        QTextLayout layout;
        const bool section = index.data(TaskLabels::Section).toBool();
        return qMax(section ? 24 : 32, qCeil(layoutText(layout, index, width)) + (section ? 8 : 10));
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override
    {
        return QSize(0, rowHeight(index, view_->viewport()->width()));
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const bool section = index.data(TaskLabels::Section).toBool();
        const bool dark = option.palette.color(QPalette::Text).lightness() > 128;
        const QRect row = option.rect;
        QTextLayout layout;
        const qreal height = layoutText(layout, index, row.width());
        const QPointF origin(row.left() + textInset(index), row.top() + (row.height() - height) / 2);
        if (section) {
            painter->save();
            painter->setPen(TaskLabels::projectTitleColor(index.data(TaskLabels::ProjectColorIndex).toInt(),
                                                         index.data(TaskLabels::ProjectColorChoice), dark));
            layout.draw(painter, origin);
            const auto last = layout.lineAt(layout.lineCount() - 1);
            const qreal x = origin.x() + last.naturalTextWidth() + 8;
            const qreal y = origin.y() + last.y() + last.height() / 2;
            const qreal endX = row.right() - 22;
            if (endX > x) {
                const QColor rule = dark ? QColor(255, 255, 255, 50) : QColor(32, 32, 32, 57);
                painter->setPen(QPen(rule, 1.0 / painter->device()->devicePixelRatioF()));
                painter->drawLine(QPointF(x, y), QPointF(endX, y));
            }
            painter->restore();
            return;
        }
        if (index.data(Qt::CheckStateRole).toInt() == Qt::Checked) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(dark ? QColor(12, 19, 26, 40) : QColor(242, 244, 246, 61));
            painter->drawRoundedRect(QRectF(option.rect).adjusted(0, 0.5, 0, -0.5), 9, 9);
            painter->restore();
        }
        QStyleOptionViewItem styled(option);
        initStyleOption(&styled, index);
        const QStyle *style = styled.widget->style();
        styled.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &styled, painter, styled.widget);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(styled.palette.color(QPalette::Text));
        layout.draw(painter, origin);
        if (index.data(TaskLabels::Next).toBool()) {
            const QPointF tip(origin.x() - 5, origin.y() + layout.lineAt(0).height() / 2);
            painter->setPen(QPen(styled.palette.color(QPalette::Text), 1.5, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(tip - QPointF(9, 0), tip);
            painter->drawLine(tip - QPointF(3.5, 3.5), tip);
            painter->drawLine(tip - QPointF(3.5, -3.5), tip);
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(TaskLabels::priorityColor(index.data(TaskLabels::Priority).toInt(), dark));
        painter->drawEllipse(QPointF(priorityTarget(row).center()), 3.5, 3.5);
        painter->restore();
    }
private:
    static int textInset(const QModelIndex &index)
    {
        if (index.data(TaskLabels::Section).toBool())
            return 25;
        return index.data(TaskLabels::Next).toBool() ? 49 : 34;
    }

    qreal layoutText(QTextLayout &layout, const QModelIndex &index, int width) const
    {
        const bool section = index.data(TaskLabels::Section).toBool();
        QFont font = view_->font();
        if (section) {
            font.setPixelSize(12);
            font.setWeight(QFont::DemiBold);
        } else {
            font.setStrikeOut(index.data(Qt::CheckStateRole).toInt() == Qt::Checked);
        }
        layout.setFont(font);
        layout.setText(index.data().toString());
        QTextOption wrap;
        wrap.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(wrap);
        layout.beginLayout();
        qreal height = 0;
        for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
            line.setLineWidth(qMax(1, width - textInset(index) - (section ? 34 : 20)));
            line.setPosition(QPointF(0, height));
            height += line.height();
        }
        layout.endLayout();
        return height;
    }
    QListWidget *view_;
};

QColor completedText(bool dark)
{
    return dark ? QColor(219, 225, 231) : QColor(47, 58, 68);
}

QString screenLabel(const QScreen *screen)
{
    const QString model = screen->model().trimmed();
    return model.isEmpty() ? screen->name() : model;
}

QString transparencyKey(bool dark)
{
    return dark ? QStringLiteral("panel/darkTransparency") : QStringLiteral("panel/lightTransparency");
}

void openMenuBelowButton(GlassMenu &menu, QWidget *button)
{
    menu.exec(button->mapToGlobal(QPoint(button->width() - menu.sizeHint().width(), button->height() + 3)));
}

void describeTask(QListWidgetItem *item)
{
    QString labels = QStringLiteral("%1 · %2")
        .arg(TaskLabels::projectName(item->data(TaskLabels::Project).toString()),
             TaskLabels::priorityName(item->data(TaskLabels::Priority).toInt()));
    if (item->data(TaskLabels::Next).toBool())
        labels.prepend(QStringLiteral("下一件 · "));
    item->setToolTip(item->text() + QChar::LineFeed + labels);
    item->setData(Qt::AccessibleDescriptionRole, labels);
}

QPixmap snapshot(QWidget *widget)
{
    const qreal scale = widget->devicePixelRatioF();
    QPixmap pixmap(widget->size() * scale);
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(Qt::transparent);
    widget->render(&pixmap, QPoint(), QRegion(), QWidget::DrawChildren);
    return pixmap;
}

} // namespace

Panel::Panel(QSettings &settings)
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                          | Qt::NoDropShadowWindowHint)
    , settingsPath_(settings.fileName())
    , expanded_(new QWidget(this))
    , handle_(new EdgeHandle(this))
    , summary_(new QLabel(expanded_))
    , handleArrow_(new QLabel(handle_))
    , handleCount_(new QLabel(handle_))
    , emptyState_(new QLabel(QStringLiteral("暂无待办"), expanded_))
    , saveError_(new QLabel(QStringLiteral("保存失败，下次修改时会重试。"), expanded_))
    , input_(new TaskInput(expanded_))
    , tasks_(new QListWidget(expanded_))
    , preferredScreen_(settings.value(QStringLiteral("panel/screen")).toStringList())
    , appearance_(settings.value(QStringLiteral("panel/appearance"), QStringLiteral("system")).toString())
    , motion_(settings.value(QStringLiteral("panel/motion"), QStringLiteral("spring")).toString())
    , tray_(new QSystemTrayIcon(QIcon(QStringLiteral(":/icons/todo.ico")), this))
    , shadow_(std::make_unique<GlassShadow>())
{
    setWindowTitle(QApplication::applicationDisplayName());
    setWindowIcon(tray_->icon());
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_QuitOnClose);
    QFont uiFont = font();
    uiFont.setFamilies({QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Segoe UI")});
    setFont(uiFont);

    auto *body = new QVBoxLayout(expanded_);
    body->setContentsMargins(panelPadding, panelPadding, panelPadding, panelPadding);
    body->setSpacing(0);

    header_ = new QHBoxLayout;
    header_->setSpacing(3);
    auto *title = new QLabel(QStringLiteral("「待办」"), expanded_);
    title->setObjectName(QStringLiteral("title"));
    summary_->setObjectName(QStringLiteral("summary"));
    summary_->setAlignment(Qt::AlignCenter);
    summary_->setFixedHeight(28);
    summary_->setMinimumWidth(34);
    header_->addWidget(title, 0, Qt::AlignVCenter);
    header_->addStretch(1);
    projectButton_ = new ProjectFilterButton(expanded_);
    projectButton_->setObjectName(QStringLiteral("projectFilter"));
    projectButton_->setCheckable(true);
    projectButton_->setFocusPolicy(Qt::TabFocus);
    projectButton_->setCursor(Qt::PointingHandCursor);
    projectButton_->setMinimumSize(44, 28);
    projectButton_->setMaximumWidth(140);
    QSizePolicy filterSize(QSizePolicy::Preferred, QSizePolicy::Preferred);
    filterSize.setHeightForWidth(true);
    projectButton_->setSizePolicy(filterSize);
    projectButton_->setIconSize(QSize(16, 16));
    header_->addWidget(projectButton_, 0, Qt::AlignVCenter);
    header_->addWidget(summary_, 0, Qt::AlignVCenter);

    collapseButton_ = new QPushButton(expanded_);
    collapseButton_->setObjectName(QStringLiteral("collapse"));
    collapseButton_->setFixedSize(29, headerHeight);
    collapseButton_->setIconSize(QSize(25, 25));
    collapseButton_->setAccessibleName(QStringLiteral("收起待办"));
    collapseButton_->setToolTip(collapseButton_->accessibleName());
    header_->addWidget(collapseButton_, 0, Qt::AlignVCenter);
    body->addLayout(header_);
    body->addSpacing(headerGap);

    tasks_->setObjectName(QStringLiteral("tasks"));
    tasks_->setAccessibleName(QStringLiteral("待办清单"));
    tasks_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    tasks_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tasks_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    tasks_->setSpacing(0);
    tasks_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    tasks_->setResizeMode(QListView::Adjust);
    tasks_->setItemDelegate(new TaskDelegate(tasks_));
    tasks_->setMouseTracking(true);
    tasks_->viewport()->installEventFilter(this);
    tasks_->setContextMenuPolicy(Qt::CustomContextMenu);
    auto *taskListStyle = new TaskListStyle;
    taskListStyle->setParent(tasks_);
    tasks_->setStyle(taskListStyle);

    emptyState_->setObjectName(QStringLiteral("empty"));
    emptyState_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    emptyState_->setTextFormat(Qt::PlainText);
    emptyState_->setWordWrap(true);
    body->addWidget(tasks_, 1);
    body->addWidget(emptyState_, 1);
    saveError_->setObjectName(QStringLiteral("saveError"));
    saveError_->hide();
    body->addWidget(saveError_);

    quickAdd_ = new QPushButton(expanded_);
    quickAdd_->setObjectName(QStringLiteral("quickAdd"));
    quickAdd_->setCursor(Qt::PointingHandCursor);
    quickAdd_->setFixedSize(34, compactFooterHeight);
    quickAdd_->setIconSize(QSize(21, 21));
    quickAdd_->setAccessibleName(QStringLiteral("快速添加待办"));
    quickAdd_->setToolTip(QStringLiteral("添加待办（Ctrl+N）"));
    composer_ = new TaskComposer(expanded_);
    composer_->setObjectName(QStringLiteral("composer"));
    composer_->setFixedHeight(composerHeight);
    auto *composer = new QHBoxLayout(composer_);
    composer->setContentsMargins(8, 4, 3, 0);
    composer->setSpacing(7);
    input_->setObjectName(QStringLiteral("taskInput"));
    input_->setPlaceholderText(QStringLiteral("添加待办"));
    input_->setAccessibleName(QStringLiteral("添加待办"));
    input_->setToolTip(QStringLiteral("Enter 保存 · Shift+Enter 保存并继续添加"));
    input_->setMaxLength(maxTaskLength);
    input_->setFixedHeight(32);
    addButton_ = new QPushButton(composer_);
    addButton_->setObjectName(QStringLiteral("add"));
    addButton_->setFixedSize(22, 22);
    addButton_->setIconSize(QSize(18, 18));
    addButton_->setAccessibleName(QStringLiteral("添加待办"));
    addButton_->setToolTip(addButton_->accessibleName());
    addButton_->setEnabled(false);
    composer->addWidget(addButton_);
    composer->addWidget(input_, 1);
    composer_->setFocusProxy(input_);
    composer_->setFocusPolicy(Qt::StrongFocus);
    priorityButton_ = new QPushButton(composer_);
    priorityButton_->setObjectName(QStringLiteral("draftPriority"));
    priorityButton_->setFixedSize(32, 28);
    priorityButton_->setIconSize(QSize(21, 16));
    for (int level = 0; level < TaskLabels::priorityCount; ++level) {
        auto *shortcut = new QShortcut(QKeySequence(Qt::ALT | (Qt::Key_1 + level)), input_);
        shortcut->setContext(Qt::WidgetShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, level] {
            draftPriority_ = level;
            updateDraftPriority();
        });
    }
    composer->addWidget(priorityButton_);
    auto *entryLayout = new QVBoxLayout;
    entryLayout->setSpacing(0);
    entryLayout->setContentsMargins(0, compactFooterGap, 0, 0);
    auto *actions = new QHBoxLayout;
    actions->addWidget(quickAdd_);
    actions->addStretch();
    undoButton_ = new QPushButton(expanded_);
    undoButton_->setObjectName(QStringLiteral("undoTask"));
    undoButton_->setCursor(Qt::PointingHandCursor);
    undoButton_->setFixedHeight(compactFooterHeight);
    undoButton_->hide();
    actions->addWidget(undoButton_);
    entryLayout->addLayout(actions);
    undoTimer_ = new QTimer(this);
    undoTimer_->setSingleShot(true);
    connect(undoTimer_, &QTimer::timeout, this, [this] {
        clearUndo();
        positionPanel();
    });
    connect(undoButton_, &QPushButton::clicked, this, &Panel::undoLastChange);
    draftTarget_ = new QLabel(expanded_);
    draftTarget_->setTextFormat(Qt::PlainText);
    draftTarget_->setObjectName(QStringLiteral("draftTarget"));
    draftTarget_->hide();
    entryLayout->addWidget(draftTarget_);
    entryLayout->addWidget(composer_);
    body->addLayout(entryLayout);
    composer_->hide();
    connect(quickAdd_, &QPushButton::clicked, this, [this] { setComposerOpen(true); });
    const auto foldIfEmpty = [this] {
        if (!composer_->isHidden() && !collapsed_ && input_->text().isEmpty() && draftPriority_ == 0)
            setComposerOpen(false);
    };
    connect(qApp, &QApplication::focusChanged, this, [this, foldIfEmpty](QWidget *, QWidget *next) {
        if (next && next->window() == this && !composer_->isAncestorOf(next)
            && next != tasks_ && !tasks_->isAncestorOf(next))
            foldIfEmpty();
    });
    connect(tasks_, &QListWidget::itemPressed, this, [foldIfEmpty](QListWidgetItem *item) {
        if (!item->data(TaskLabels::Section).toBool())
            foldIfEmpty();
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [foldIfEmpty](Qt::ApplicationState state) {
        if (state == Qt::ApplicationInactive)
            foldIfEmpty();
    });
    auto *quickShortcut = new QShortcut(QKeySequence::New, expanded_);
    quickShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(quickShortcut, &QShortcut::activated, this, [this] { setComposerOpen(true); });

    handle_->setObjectName(QStringLiteral("handle"));
    handle_->setContextMenuPolicy(Qt::CustomContextMenu);
    auto *handleLayout = new QVBoxLayout(handle_);
    handleLayout->setContentsMargins(1, 0, 1, 0);
    handleLayout->setSpacing(4);
    handleLayout->setAlignment(Qt::AlignCenter);
    handleArrow_->setFixedSize(16, 16);
    handleArrow_->setAttribute(Qt::WA_TransparentForMouseEvents);
    handleCount_->setObjectName(QStringLiteral("handleCount"));
    handleCount_->setAlignment(Qt::AlignCenter);
    handleCount_->setFixedHeight(18);
    handleCount_->setMinimumWidth(18);
    handleCount_->setAttribute(Qt::WA_TransparentForMouseEvents);
    handleLayout->addWidget(handleArrow_, 0, Qt::AlignHCenter);
    handleLayout->addWidget(handleCount_, 0, Qt::AlignHCenter);
    handle_->hide();

    setTabOrder(input_, addButton_);
    connect(projectButton_, &QPushButton::clicked, this, [this] {
        GlassMenu menu(this, expanded_, dark_, transparency_);
        populateProjectMenu(menu);
        openMenuBelowButton(menu, projectButton_);
    });
    connect(priorityButton_, &QPushButton::clicked, this, [this] {
        GlassMenu menu(this, expanded_, dark_, transparency_);
        menu.addSection(QStringLiteral("紧急程度"));
        populatePriorityMenu(menu);
        openMenuBelowButton(menu, priorityButton_);
    });
    connect(collapseButton_, &QPushButton::clicked, this, [this] { setCollapsed(true); });
    connect(handle_, &QPushButton::clicked, this, [this] { setCollapsed(false); });
    connect(handle_, &QWidget::customContextMenuRequested, this, [this] { showPanelMenu(handle_); });
    connect(input_, &QLineEdit::returnPressed, this, [this] {
        addTask(static_cast<TaskInput *>(input_)->continueEntry());
    });
    input_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(input_, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        const QPoint global = input_->mapToGlobal(point);
        std::unique_ptr<QMenu> editing(input_->createStandardContextMenu());
        GlassMenu menu(this, expanded_, dark_, transparency_);
        menu.addActions(editing->actions());
        addPanelSettingsAction(menu, global);
        menu.exec(global);
    });
    connect(input_, &QLineEdit::textChanged, addButton_,
            [this](const QString &text) { addButton_->setEnabled(!text.trimmed().isEmpty()); });
    connect(addButton_, &QPushButton::clicked, this, [this] { addTask(); });
    connect(tasks_, &QListWidget::customContextMenuRequested, this, &Panel::showTaskMenu);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Context) {
            showPanelMenu(nullptr);
        } else if (reason == QSystemTrayIcon::Trigger && !panelIsShowing()) {
            collapsed_ = false;
            restorePanel();
        } else if (reason == QSystemTrayIcon::Trigger)
            setCollapsed(!collapsed_);
    });

    if (appearance_ != QStringLiteral("light") && appearance_ != QStringLiteral("dark"))
        appearance_ = QStringLiteral("system");
    unfinishedOnly_ = settings.value(QStringLiteral("panel/unfinishedOnly"), false).toBool();
    applyAppearance();
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (appearance_ == QStringLiteral("system"))
            applyAppearance();
    });

    projects_ = settings.value(QStringLiteral("projects/names")).toStringList();
    projectColors_ = settings.value(QStringLiteral("projects/titleColors")).toMap();
    foldedProjects_ = settings.value(QStringLiteral("projects/folded")).toStringList();
    projects_.removeAll(QString());
    projects_.removeDuplicates();
    const int count = settings.beginReadArray(QStringLiteral("tasks"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const auto text = settings.value(QStringLiteral("title")).toString().trimmed();
        if (!text.isEmpty()) {
            const QString project = settings.value(QStringLiteral("project")).toString();
            if (!project.isEmpty() && !projects_.contains(project))
                projects_.append(project);
            const bool done = settings.value(QStringLiteral("done"), false).toBool();
            auto *item = appendTask(text, done, project, settings.value(QStringLiteral("priority"), 0).toInt());
            if (!done && !nextTask_ && settings.value(QStringLiteral("next"), false).toBool())
                setNextTask(item);
        }
    }
    settings.endArray();
    if (settings.contains(QStringLiteral("panel/projectFilter"))) {
        const QString project = settings.value(QStringLiteral("panel/projectFilter")).toString();
        if (project.isEmpty() || projects_.contains(project))
            projectFilter_ = project;
    }
    connect(tasks_, &QListWidget::itemChanged, this, &Panel::updateTask);
    updateSummary();
    tray_->show();

    recoveryTimer_.setSingleShot(true);
    recoveryTimer_.setInterval(250);
    connect(&recoveryTimer_, &QTimer::timeout, this, &Panel::restorePanel);
    visibilityTimer_.setInterval(60000);
    connect(&visibilityTimer_, &QTimer::timeout, this, [this] {
        if (!panelIsShowing())
            restorePanel();
        else
            registerSessionNotifications();
    });
    for (auto *screen : QGuiApplication::screens())
        watchScreen(screen);
    connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
        watchScreen(screen);
        schedulePanelRecovery();
    });
    connect(qApp, &QGuiApplication::screenRemoved, this, &Panel::schedulePanelRecovery);
    connect(qApp, &QGuiApplication::primaryScreenChanged, this, &Panel::schedulePanelRecovery);
    winId();
    // The native window can still have its old DPI during the first move.
    connect(windowHandle(), &QWindow::screenChanged, this, &Panel::schedulePanelRecovery);
    transition_.setEasingCurve(QEasingCurve::Linear);
    connect(&transition_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        openness_ = value.toReal();
        positionPanel();
        update();
    });
    connect(&transition_, &QVariantAnimation::finished, this, [this] { finishTransition(); });
    positionPanel();
}

Panel::~Panel()
{
    visibilityTimer_.stop();
    recoveryTimer_.stop();
    unregisterSessionNotifications();
}

void Panel::applyAppearance()
{
    dark_ = appearance_ == QStringLiteral("dark")
        || (appearance_ == QStringLiteral("system") && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    const QSettings prefs(settingsPath_, QSettings::IniFormat);
    const int defaultTransparency = dark_ ? 23 : 51;
    bool valid = false;
    const int percent = prefs.value(transparencyKey(dark_), defaultTransparency).toInt(&valid);
    setTransparency(valid ? qBound(0, percent, 100) : defaultTransparency);
    QString sheet = QString::fromLatin1(lightPanelStyle);
    if (dark_)
        sheet += QString::fromLatin1(darkPanelStyle);
    setStyleSheet(sheet);
    collapseButton_->setIcon(controlIcon(Icon::Collapse, dark_));
    projectButton_->setIcon(controlIcon(Icon::Filter, dark_));
    addButton_->setIcon(controlIcon(Icon::Add, dark_));
    quickAdd_->setIcon(controlIcon(Icon::QuickAdd, dark_));
    updateDraftPriority();
    handleArrow_->setPixmap(controlIcon(Icon::Expand, dark_).pixmap(QSize(16, 16), devicePixelRatioF()));
    const QSignalBlocker block(tasks_);
    for (int index = 0; index < tasks_->count(); ++index) {
        auto *item = tasks_->item(index);
        if (item->checkState() == Qt::Checked)
            item->setForeground(completedText(dark_));
    }
    if (isVisible())
        positionPanel();
    update();
}

void Panel::setTransparency(int percent)
{
    transparency_ = percent;
    shadow_->setTransparency(percent);
    for (auto *menu : findChildren<QMenu *>()) {
        if (auto *glass = dynamic_cast<GlassMenu *>(menu))
            glass->setTransparency(percent);
    }
    update();
}

void Panel::addTransparencyControl(QMenu *menu)
{
    auto *control = new QWidget(menu);
    control->setMinimumWidth(184);
    auto *layout = new QVBoxLayout(control);
    layout->setContentsMargins(10, 4, 10, 6);
    layout->setSpacing(2);
    auto *labels = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("透明度"), control);
    auto *value = new QLabel(QStringLiteral("%1%").arg(transparency_), control);
    labels->addWidget(title);
    labels->addStretch();
    labels->addWidget(value);
    layout->addLayout(labels);

    auto *slider = new QSlider(Qt::Horizontal, control);
    slider->setObjectName(QStringLiteral("glassTransparency"));
    slider->setAccessibleName(title->text());
    slider->setToolTip(QStringLiteral("数值越高，背景越通透。最高档仍保留衬底，方便阅读。"));
    title->setBuddy(slider);
    slider->setRange(0, 100);
    slider->setValue(transparency_);
    slider->setFixedHeight(24);
    slider->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: %1; }"
        "QSlider::sub-page:horizontal { background: %2; border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 14px; margin: -5px 0; border-radius: 7px;"
        " background: #f8fafb; border: 1px solid #8a98a2; }"
        "QSlider:focus::handle:horizontal { border-color: #507899; }")
        .arg(dark_ ? "#58616a" : "#c1c8cc", dark_ ? "#becdd7" : "#738d9e"));
    layout->addWidget(slider);
    control->setFocusProxy(slider);
    auto *action = new QWidgetAction(menu);
    action->setDefaultWidget(control);
    menu->addAction(action);

    const QString key = transparencyKey(dark_);
    const auto saveValue = [this, key, slider] {
        QSettings prefs(settingsPath_, QSettings::IniFormat);
        prefs.setValue(key, slider->value());
    };
    connect(slider, &QSlider::valueChanged, this, [this, slider, value, saveValue](int percent) {
        setTransparency(percent);
        value->setText(QStringLiteral("%1%").arg(percent));
        if (!slider->isSliderDown())
            saveValue();
    });
    connect(slider, &QSlider::sliderReleased, this, saveValue);
    connect(menu, &QMenu::aboutToHide, slider, [slider, saveValue] {
        if (slider->isSliderDown())
            saveValue();
    });
}

void Panel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QRectF bounds(rect());
    paintGlass(painter, bounds, openness_, dark_, transparency_);
    if (transition_.state() == QAbstractAnimation::Running) {
        painter.setClipPath(glassOutline(bounds, openness_));
        const qreal content = smoothProgress((openness_ - 0.20) / 0.68);
        painter.setOpacity(content);
        const qreal slide = (1 - content) * 16;
        painter.drawPixmap(QPointF(slide, 0), expandedSnapshot_);
        painter.setOpacity(1 - smoothProgress((openness_ - 0.19) / 0.37));
        painter.drawPixmap(QPointF((width() - handleWidth) / 2.0, (height() - handleHeight) / 2.0), handleSnapshot_);
    }
}

void Panel::resizeEvent(QResizeEvent *)
{
    if (transition_.state() != QAbstractAnimation::Running) {
        if (collapsed_)
            handle_->setGeometry(rect());
        else
            expanded_->setGeometry(rect());
    }
}

void Panel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    enableWindowBackdrop(winId());
    positionPanel();
    registerSessionNotifications();
    visibilityTimer_.start();
    shadow_->show();
    placeWindowBelow(shadow_->winId(), winId());
    update();
}

void Panel::closeEvent(QCloseEvent *event)
{
    QWidget::closeEvent(event);
    visibilityTimer_.stop();
    recoveryTimer_.stop();
}

bool Panel::event(QEvent *event)
{
    const bool handled = QWidget::event(event);
    if (event->type() == QEvent::WinIdChange)
        schedulePanelRecovery();
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if (event->type() == QEvent::DevicePixelRatioChange)
        schedulePanelRecovery();
#endif
    return handled;
}

bool Panel::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        const auto &msg = *static_cast<const MSG *>(message);
        if (msg.message == WM_NCDESTROY && reinterpret_cast<WId>(msg.hwnd) == sessionWindow_)
            unregisterSessionNotifications();
        const bool reconnect = msg.message == WM_WTSSESSION_CHANGE
            && (msg.wParam == WTS_SESSION_UNLOCK || msg.wParam == WTS_CONSOLE_CONNECT
                || msg.wParam == WTS_REMOTE_CONNECT);
        const bool resume = msg.message == WM_POWERBROADCAST
            && (msg.wParam == PBT_APMRESUMEAUTOMATIC || msg.wParam == PBT_APMRESUMESUSPEND);
        if (reconnect || resume || msg.message == WM_DISPLAYCHANGE || msg.message == WM_DWMCOMPOSITIONCHANGED)
            schedulePanelRecovery();
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

void Panel::registerSessionNotifications()
{
#ifdef Q_OS_WIN
    const WId current = internalWinId();
    if (sessionWindow_ == current)
        return;
    unregisterSessionNotifications();
    if (current && WTSRegisterSessionNotification(reinterpret_cast<HWND>(current), NOTIFY_FOR_THIS_SESSION))
        sessionWindow_ = current;
#endif
}

void Panel::unregisterSessionNotifications()
{
#ifdef Q_OS_WIN
    if (sessionWindow_ && IsWindow(reinterpret_cast<HWND>(sessionWindow_)))
        WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(sessionWindow_));
    sessionWindow_ = 0;
#endif
}

bool Panel::panelIsShowing() const
{
    const auto *target = targetScreen();
    const auto *pane = collapsed_ ? static_cast<const QWidget *>(handle_) : expanded_;
    return isVisible() && target && windowHandle() && windowHandle()->screen() == target
        && target->availableGeometry().contains(geometry()) && panelWindowOnTop(internalWinId())
        && (transition_.state() == QAbstractAnimation::Running || pane->isVisible());
}

void Panel::schedulePanelRecovery()
{
    if (visibilityTimer_.isActive())
        recoveryTimer_.start();
}

void Panel::restorePanel()
{
    const auto *target = targetScreen();
    if (!target || target->availableGeometry().isEmpty())
        return;
    transition_.stop();
    if (isMinimized())
        setWindowState(windowState() & ~Qt::WindowMinimized);
    if (!isVisible())
        show();
    finishTransition(false);
    restorePanelWindow(winId(), mask(), devicePixelRatioF());
    registerSessionNotifications();
    if (!shadow_->isVisible())
        shadow_->show();
    placeWindowBelow(shadow_->winId(), winId());
    update();
}

void Panel::hideEvent(QHideEvent *event)
{
    shadow_->hide();
    QWidget::hideEvent(event);
}

void Panel::watchScreen(QScreen *screen)
{
    connect(screen, &QScreen::availableGeometryChanged, this, &Panel::schedulePanelRecovery);
    connect(screen, &QScreen::logicalDotsPerInchChanged, this, &Panel::schedulePanelRecovery);
}

QSize Panel::expandedSize(const QRect &workArea) const
{
    const int width = std::min(panelWidth, workArea.width() - panelMargin);
    const int maxPanelHeight = workArea.height() * 2 / 3;
    int footer = composer_->isHidden() ? compactFooterHeight : composerHeight;
    if (!composer_->isHidden() && !undoButton_->isHidden())
        footer += compactFooterHeight;
    if (!draftTarget_->isHidden())
        footer += draftTarget_->sizeHint().height();
    const int topHeight = qMax(headerHeight, header_->heightForWidth(width - 2 * panelPadding));
    const int controlsHeight = 2 * panelPadding + topHeight + headerGap + compactFooterGap
        + footer + (saveError_->isHidden() ? 0 : saveError_->sizeHint().height());
    const int maxListHeight = std::max(0, maxPanelHeight - controlsHeight);
    int listHeight = visibleTaskCount_ ? 0 : std::max(emptyListHeight, emptyState_->heightForWidth(width - 2 * panelPadding));
    const auto *delegate = static_cast<TaskDelegate *>(tasks_->itemDelegate());
    for (int row = 0; visibleTaskCount_ && row < tasks_->count() && listHeight < maxListHeight; ++row) {
        const auto *item = tasks_->item(row);
        if (!item->isHidden())
            listHeight += delegate->rowHeight(tasks_->indexFromItem(item), width - 2 * panelPadding);
    }
    return QSize(width, std::min(controlsHeight + listHeight, maxPanelHeight));
}

QScreen *Panel::targetScreen() const
{
    const QString name = preferredScreen_.value(0);
    const QString serial = preferredScreen_.value(1);
    QScreen *serialMatch = nullptr;
    for (auto *screen : QGuiApplication::screens()) {
        const bool matchesSerial = !serial.isEmpty() && screen->serialNumber() == serial;
        if (screen->name() == name && (serial.isEmpty() || matchesSerial))
            return screen;
        if (matchesSerial && !serialMatch)
            serialMatch = screen;
    }
    return serialMatch ? serialMatch : QGuiApplication::primaryScreen();
}

void Panel::positionPanel()
{
    auto *target = targetScreen();
    if (!target || target->availableGeometry().isEmpty())
        return;

    // Set the target first, so Qt maps the geometry using that screen's DPI.
    const bool screenChanged = windowHandle()->screen() != target;
    if (screenChanged)
        windowHandle()->setScreen(target);

    const QRect area = target->availableGeometry();
    const auto expanded = expandedSize(area);
    // Dock first, then release a four-pixel squeeze. Both directions follow
    // the same continuous path, including when a click reverses the motion.
    constexpr qreal dock = 0.21;
    const qreal travel = smoothProgress((openness_ - dock) / (1 - dock));
    const qreal squeeze = (openness_ < dock ? smoothProgress(openness_ / dock) : 1 - travel);
    const int sideMargin = qRound(panelMargin * travel);
    const int width = std::min(handleWidth + qRound((expanded.width() - handleWidth) * travel - 4 * squeeze),
                               area.width() - sideMargin);
    const int height = std::min(handleHeight + qRound((expanded.height() - handleHeight)
                                * smoothProgress(openness_) + 3 * squeeze),
                                area.height() - panelMargin);
    setGeometry(area.right() - sideMargin - width + 1, area.top() + panelMargin, width, height);
    const auto core = glassCoreRegion(size(), openness_);
    if (mask() != core)
        setMask(core);
    shadow_->positionBehind(geometry(), target, openness_, dark_, core);
    if (screenChanged && shadow_->isVisible())
        placeWindowBelow(shadow_->winId(), winId());
}

void Panel::setCollapsed(bool collapsed)
{
    if (collapsed_ == collapsed)
        return;
    collapsed_ = collapsed;

    const bool reversing = transition_.state() == QAbstractAnimation::Running;
    transition_.stop();
    if (motion_ == QStringLiteral("off") || !windowHandle()->screen()
        || (motion_ == QStringLiteral("system") && !windowAnimationsEnabled())) {
        finishTransition();
        return;
    }

    if (!reversing) {
        expanded_->resize(expandedSize(windowHandle()->screen()->availableGeometry()));
        handle_->resize(handleWidth, handleHeight);
        expandedSnapshot_ = snapshot(expanded_);
        handleSnapshot_ = snapshot(handle_);
        expanded_->hide();
        handle_->hide();
    }

    // Animate the shape around frozen content so no widget has to reflow during the transition.
    const QSignalBlocker transitionSignalsDuringSetup(&transition_);
    transition_.setStartValue(openness_);
    transition_.setEndValue(collapsed_ ? 0.0 : 1.0);
    transition_.setDuration(qMax(1, qRound(transitionDuration * qAbs(openness_ - transition_.endValue().toReal()))));
    transition_.start();
    update();
}

void Panel::finishTransition(bool restoreFocus)
{
    openness_ = collapsed_ ? 0.0 : 1.0;
    expandedSnapshot_ = QPixmap();
    handleSnapshot_ = QPixmap();
    if (collapsed_)
        expanded_->hide();
    else
        handle_->hide();
    positionPanel();
    QWidget *visiblePane = collapsed_ ? static_cast<QWidget *>(handle_) : expanded_;
    visiblePane->setGeometry(rect());
    visiblePane->show();
    if (restoreFocus && isActiveWindow()) {
        if (collapsed_)
            handle_->setFocus(Qt::OtherFocusReason);
        else if (!composer_->isHidden())
            input_->setFocus(Qt::OtherFocusReason);
        else
            quickAdd_->setFocus(Qt::OtherFocusReason);
    }
    update();
}

void Panel::contextMenuEvent(QContextMenuEvent *event)
{
    showPanelMenu(this, event->globalPos());
    event->accept();
}

void Panel::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && !composer_->isHidden() && !collapsed_) {
        setComposerOpen(false);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void Panel::addPanelSettingsAction(QMenu &menu, const QPoint &globalPosition)
{
    menu.addSeparator();
    menu.addAction(QStringLiteral("面板设置"), this, [this, globalPosition] {
        QTimer::singleShot(0, this, [this, globalPosition] { showPanelMenu(this, globalPosition); });
    });
}

void Panel::showPanelMenu(QWidget *anchor, std::optional<QPoint> requestedPosition)
{
    GlassMenu menu(this, expanded_, dark_, transparency_);
    menu.setToolTipsVisible(true);
    menu.addSection(QStringLiteral("显示器"));
    auto *automatic = menu.addAction(QStringLiteral("跟随主屏"));
    automatic->setCheckable(true);
    automatic->setChecked(preferredScreen_.isEmpty());
    connect(automatic, &QAction::triggered, this, [this] {
        preferredScreen_.clear();
        QSettings settings(settingsPath_, QSettings::IniFormat);
        settings.remove(QStringLiteral("panel/screen"));
        settings.sync();
        positionPanel();
    });

    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        auto *screen = screens.at(i);
        const QString label = QStringLiteral("屏幕 %1  ·  %2").arg(i + 1).arg(screenLabel(screen));
        const QStringList identity{screen->name(), screen->serialNumber()};
        auto *action = menu.addAction(label);
        const QSize pixels = screen->size() * screen->devicePixelRatio();
        action->setToolTip(QStringLiteral("%1 × %2").arg(pixels.width()).arg(pixels.height()));
        action->setCheckable(true);
        action->setChecked(preferredScreen_ == identity);
        connect(action, &QAction::triggered, this, [this, identity] {
            preferredScreen_ = identity;
            QSettings settings(settingsPath_, QSettings::IniFormat);
            settings.setValue(QStringLiteral("panel/screen"), preferredScreen_);
            settings.sync();
            positionPanel();
        });
    }

    menu.addSeparator();
    auto *appearances = new GlassMenu(&menu, expanded_, dark_, transparency_);
    appearances->setTitle(QStringLiteral("外观"));
    menu.addMenu(appearances);
    auto *appearanceChoices = new QActionGroup(appearances);
    const std::pair<QString, QString> modes[] = {
        {QStringLiteral("system"), QStringLiteral("跟随系统")},
        {QStringLiteral("light"), QStringLiteral("浅色玻璃")},
        {QStringLiteral("dark"), QStringLiteral("深色玻璃")},
    };
    for (const auto &[mode, label] : modes) {
        auto *action = appearances->addAction(label);
        action->setCheckable(true);
        action->setChecked(appearance_ == mode);
        appearanceChoices->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode] {
            appearance_ = mode;
            QSettings settings(settingsPath_, QSettings::IniFormat);
            settings.setValue(QStringLiteral("panel/appearance"), appearance_);
            settings.sync();
            applyAppearance();
        });
    }
    appearances->addSeparator();
    addTransparencyControl(appearances);
    auto *motionMenu = new GlassMenu(appearances, expanded_, dark_, transparency_);
    motionMenu->setTitle(QStringLiteral("贴边动效"));
    appearances->addMenu(motionMenu);
    const std::pair<QString, QString> motions[] = {
        {QStringLiteral("spring"), QStringLiteral("轻弹")},
        {QStringLiteral("system"), QStringLiteral("跟随系统")},
        {QStringLiteral("off"), QStringLiteral("关闭动画")},
    };
    for (const auto &[mode, label] : motions) {
        auto *action = motionMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(motion_ == mode);
        connect(action, &QAction::triggered, this, [this, mode] {
            motion_ = mode;
            QSettings(settingsPath_, QSettings::IniFormat).setValue(QStringLiteral("panel/motion"), mode);
        });
    }
    auto *unfinished = menu.addAction(QStringLiteral("只看未完成"));
    unfinished->setCheckable(true);
    unfinished->setChecked(unfinishedOnly_);
    connect(unfinished, &QAction::triggered, this, [this](bool checked) {
        unfinishedOnly_ = checked;
        QSettings settings(settingsPath_, QSettings::IniFormat);
        settings.setValue(QStringLiteral("panel/unfinishedOnly"), checked);
        updateSummary();
    });
    if (!anchor) {
        menu.addSeparator();
        menu.addAction(QStringLiteral("快速添加待办"), this, [this] {
            if (!panelIsShowing()) {
                collapsed_ = false;
                restorePanel();
            }
            activateWindow();
            setComposerOpen(true);
            if (collapsed_)
                setCollapsed(false);
        });
        menu.addAction(collapsed_ ? QStringLiteral("展开面板") : QStringLiteral("收起面板"),
                       this, [this] { setCollapsed(!collapsed_); });
#ifdef Q_OS_WIN
        menu.addAction(QStringLiteral("Windows 显示设置…"), this, [] {
            QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:display")));
        });
#endif
    }
    menu.addAction(QStringLiteral("退出待办"), qApp, &QApplication::quit);
    const QPoint position = anchor ? mapToGlobal(QPoint(width() - menu.sizeHint().width(), height() + 5))
                                   : QCursor::pos();
    menu.exec(requestedPosition.value_or(position));
}

bool Panel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != tasks_->viewport())
        return QWidget::eventFilter(watched, event);

    const auto priorityItemAt = [this](const QPoint &point) -> QListWidgetItem * {
        auto *item = tasks_->itemAt(point);
        if (item && !item->data(TaskLabels::Section).toBool()
            && priorityTarget(tasks_->visualItemRect(item)).contains(point))
            return item;
        return nullptr;
    };
    switch (event->type()) {
    case QEvent::MouseMove: {
        const QPoint point = static_cast<QMouseEvent *>(event)->position().toPoint();
        if (priorityItemAt(point))
            tasks_->viewport()->setCursor(Qt::PointingHandCursor);
        else
            tasks_->viewport()->unsetCursor();
        break;
    }
    case QEvent::Leave:
        tasks_->viewport()->unsetCursor();
        break;
    case QEvent::ToolTip: {
        auto *help = static_cast<QHelpEvent *>(event);
        if (auto *item = priorityItemAt(help->pos())) {
            const QString tip = QStringLiteral("紧急程度：%1，点击修改")
                                    .arg(TaskLabels::priorityName(item->data(TaskLabels::Priority).toInt()));
            QToolTip::showText(help->globalPos(), tip, tasks_->viewport(), priorityTarget(tasks_->visualItemRect(item)));
            return true;
        }
        break;
    }
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease: {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton)
            break;
        auto *item = priorityItemAt(mouse->position().toPoint());
        if (event->type() != QEvent::MouseButtonRelease) {
            pressedPriority_ = tasks_->indexFromItem(item);
            if (item)
                return true;
        } else if (pressedPriority_.isValid()) {
            const bool sameItem = item && pressedPriority_ == tasks_->indexFromItem(item);
            pressedPriority_ = QModelIndex();
            if (sameItem) {
                GlassMenu menu(this, expanded_, dark_, transparency_);
                menu.addSection(QStringLiteral("紧急程度"));
                populatePriorityMenu(menu, item);
                const QRect target = priorityTarget(tasks_->visualItemRect(item)).intersected(tasks_->viewport()->rect());
                const QPoint anchor(target.right() + 1 - menu.sizeHint().width(), target.bottom() + 3);
                menu.exec(tasks_->viewport()->mapToGlobal(anchor));
            }
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

void Panel::showTaskMenu(const QPoint &point)
{
    auto *item = tasks_->itemAt(point);
    const QPoint global = tasks_->viewport()->mapToGlobal(point);
    if (!item) {
        showPanelMenu(this, global);
        return;
    }
    if (item->data(TaskLabels::Section).toBool()) {
        showProjectColorMenu(item->data(TaskLabels::Project).toString(), global);
        return;
    }

    tasks_->setCurrentItem(item);
    GlassMenu menu(this, expanded_, dark_, transparency_);
    menu.addAction(QStringLiteral("编辑"), this, [this, item] { tasks_->editItem(item); });
    if (item->checkState() != Qt::Checked) {
        menu.addAction(item == nextTask_ ? QStringLiteral("取消下一件") : QStringLiteral("设为下一件"),
                       this, [this, item] {
            setNextTask(item == nextTask_ ? nullptr : item);
            saveTasks();
        });
    }
    const int previous = adjacentVisibleRow(item, -1);
    const int next = adjacentVisibleRow(item, 1);
    auto *up = menu.addAction(QStringLiteral("上移"), this, [this, item, previous] { moveTask(item, previous); });
    up->setEnabled(previous >= 0);
    auto *down = menu.addAction(QStringLiteral("下移"), this, [this, item, next] { moveTask(item, next); });
    down->setEnabled(next >= 0);
    menu.addSeparator();
    auto *projectMenu = new GlassMenu(&menu, expanded_, dark_, transparency_);
    projectMenu->setTitle(QStringLiteral("所属项目"));
    populateProjectMenu(*projectMenu, item);
    menu.addMenu(projectMenu);
    auto *priorityMenu = new GlassMenu(&menu, expanded_, dark_, transparency_);
    priorityMenu->setTitle(QStringLiteral("紧急程度"));
    populatePriorityMenu(*priorityMenu, item);
    menu.addMenu(priorityMenu);
    menu.addSeparator();
    menu.addAction(QStringLiteral("删除"), this, [this, item] { deleteTask(item); });
    addPanelSettingsAction(menu, global);
    menu.exec(global);
}

void Panel::setComposerOpen(bool open)
{
    const bool returnFocus = composer_->isAncestorOf(QApplication::focusWidget());
    quickAdd_->setVisible(!open);
    composer_->setVisible(open);
    if (!open && input_->text().isEmpty())
        draftProject_.reset();
    updateDraftTarget();
    if (!collapsed_ && transition_.state() != QAbstractAnimation::Running)
        positionPanel();
    if (open) {
        if (!isActiveWindow())
            activateWindow();
        input_->setFocus(Qt::ShortcutFocusReason);
    } else if (returnFocus)
        quickAdd_->setFocus(Qt::OtherFocusReason);
}

void Panel::updateDraftTarget()
{
    draftTarget_->setVisible(draftProject_.has_value() && !composer_->isHidden());
    if (draftProject_) {
        const QString label = QStringLiteral("添加到 · %1").arg(TaskLabels::projectName(*draftProject_));
        draftTarget_->setToolTip(label);
        draftTarget_->setText(draftTarget_->fontMetrics().elidedText(label, Qt::ElideRight,
                                                                      panelWidth - 2 * panelPadding - 8));
    }
}

void Panel::addTask(bool continueEntry)
{
    const auto title = input_->text().trimmed();
    if (title.isEmpty())
        return;
    auto *added = appendTask(title, false, draftProject_.value_or(projectFilter_.value_or(QString())), draftPriority_);
    draftPriority_ = 0;
    updateDraftPriority();
    input_->clear();
    if (!continueEntry)
        setComposerOpen(false);
    saveTasks();
    tasks_->scrollToItem(added->isHidden() ? sections_.value(added->data(TaskLabels::Project).toString()) : added);
}

QListWidgetItem *Panel::appendTask(const QString &title, bool done, const QString &project, int priority)
{
    auto *item = new QListWidgetItem(title);
    item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable);
    item->setData(savedTitleRole, title);
    item->setData(TaskLabels::Project, project);
    item->setData(TaskLabels::Priority, TaskLabels::validPriority(priority));
    item->setData(TaskLabels::SavedDone, done);
    describeTask(item);
    item->setCheckState(done ? Qt::Checked : Qt::Unchecked);
    if (done) {
        QFont font = tasks_->font();
        font.setStrikeOut(true);
        item->setFont(font);
        item->setForeground(completedText(dark_));
    }
    insertInProject(item);
    return item;
}

void Panel::insertInProject(QListWidgetItem *item, int groupOffset)
{
    const QString project = item->data(TaskLabels::Project).toString();
    auto *heading = sections_.value(project);
    if (!heading) {
        heading = new QListWidgetItem(TaskLabels::projectName(project));
        heading->setFlags(Qt::ItemIsEnabled);
        heading->setData(TaskLabels::Section, true);
        heading->setData(TaskLabels::Project, project);
        heading->setData(TaskLabels::ProjectColorIndex, int(projects_.indexOf(project)));
        heading->setData(TaskLabels::ProjectColorChoice, projectColors_.value(project));
        heading->setToolTip(heading->text());
        sections_.insert(project, heading);
        tasks_->addItem(heading);
        auto *toggle = new SectionToggleButton;
        toggle->setChecked(foldedProjects_.contains(project));
        toggle->setAccessibleName(QStringLiteral("折叠或展开 · %1").arg(heading->text()));
        toggle->setToolTip(toggle->accessibleName() + QStringLiteral("\n右键设置项目颜色"));
        tasks_->setItemWidget(heading, toggle);
        auto *add = toggle->add;
        add->setObjectName(QStringLiteral("sectionAdd"));
        add->setAccessibleName(QStringLiteral("添加到 · %1").arg(heading->text()));
        add->setToolTip(add->accessibleName() + QStringLiteral("\n右键设置项目颜色"));
        add->setFocusPolicy(Qt::TabFocus);
        add->setCursor(Qt::PointingHandCursor);
        toggle->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(toggle, &QWidget::customContextMenuRequested, this, [this, toggle, project](const QPoint &point) {
            showProjectColorMenu(project, toggle->mapToGlobal(point));
        });
        connect(toggle, &QPushButton::toggled, this, [this, project](bool folded) {
            if (folded)
                foldedProjects_.append(project);
            else
                foldedProjects_.removeAll(project);
            saveTasks();
        });
        connect(add, &QPushButton::clicked, this, [this, project] {
            if (!composer_->isHidden() && draftProject_ == project) {
                input_->setFocus(Qt::MouseFocusReason);
                return;
            }
            draftProject_ = project;
            setComposerOpen(true);
        });
    }
    const int first = tasks_->row(heading) + 1;
    int row = first;
    while (row < tasks_->count() && (groupOffset < 0 || row - first < groupOffset)
           && !tasks_->item(row)->data(TaskLabels::Section).toBool())
        ++row;
    tasks_->insertItem(row, item);
}

void Panel::assignProject(QListWidgetItem *item, const QString &project)
{
    if (item->data(TaskLabels::Project).toString() == project)
        return;
    tasks_->takeItem(tasks_->row(item));
    item->setData(TaskLabels::Project, project);
    describeTask(item);
    insertInProject(item);
    saveTasks();
    if (!item->isHidden()) {
        tasks_->setCurrentItem(item);
        tasks_->scrollToItem(item);
    }
}

void Panel::updateTask(QListWidgetItem *item)
{
    if (item->data(TaskLabels::Section).toBool())
        return;
    const bool done = item->checkState() == Qt::Checked;
    if (done != item->data(TaskLabels::SavedDone).toBool()) {
        if (done) {
            clearUndo();
            completedUndo_ = item;
            undoWasNext_ = item == nextTask_;
            if (undoWasNext_)
                setNextTask(nullptr);
            offerUndo(QStringLiteral("已完成"));
        } else if (completedUndo_ == item) {
            clearUndo();
        }
    }
    {
        const QSignalBlocker block(tasks_);
        item->setData(TaskLabels::SavedDone, done);
        auto title = item->text().trimmed().left(maxTaskLength);
        if (title.isEmpty())
            title = item->data(savedTitleRole).toString();
        item->setText(title);
        item->setData(savedTitleRole, title);
        describeTask(item);
        QFont font = tasks_->font();
        font.setStrikeOut(done);
        item->setFont(font);
        if (done)
            item->setForeground(completedText(dark_));
        else
            item->setForeground(QBrush());
    }
    saveTasks();
}

void Panel::setNextTask(QListWidgetItem *item)
{
    const QSignalBlocker block(tasks_);
    if (nextTask_) {
        nextTask_->setData(TaskLabels::Next, false);
        describeTask(nextTask_);
    }
    nextTask_ = item;
    if (item) {
        item->setData(TaskLabels::Next, true);
        describeTask(item);
    }
}

void Panel::clearUndo()
{
    undoTimer_->stop();
    undoButton_->hide();
    completedUndo_ = nullptr;
    deletedUndo_.reset();
    undoWasNext_ = false;
}

void Panel::offerUndo(const QString &message)
{
    undoButton_->setText(message + QStringLiteral(" · 撤销"));
    undoButton_->show();
    undoTimer_->start(8000);
}

void Panel::deleteTask(QListWidgetItem *item)
{
    clearUndo();
    undoWasNext_ = item == nextTask_;
    if (undoWasNext_)
        setNextTask(nullptr);
    const int row = tasks_->row(item);
    auto *heading = sections_.value(item->data(TaskLabels::Project).toString());
    deletedGroupOffset_ = row - tasks_->row(heading) - 1;
    deletedUndo_.reset(tasks_->takeItem(row));
    offerUndo(QStringLiteral("已删除"));
    saveTasks();
}

void Panel::undoLastChange()
{
    const bool restoreNext = undoWasNext_ && !nextTask_;
    auto *item = completedUndo_;
    if (deletedUndo_) {
        item = deletedUndo_.release();
        insertInProject(item, deletedGroupOffset_);
    } else if (item) {
        const QSignalBlocker block(tasks_);
        item->setCheckState(Qt::Unchecked);
    }
    clearUndo();
    if (item) {
        if (restoreNext)
            setNextTask(item);
        updateTask(item);
        if (!item->isHidden())
            tasks_->scrollToItem(item);
    }
}

int Panel::adjacentVisibleRow(QListWidgetItem *item, int direction) const
{
    for (int i = tasks_->row(item) + direction; i >= 0 && i < tasks_->count(); i += direction) {
        if (tasks_->item(i)->data(TaskLabels::Section).toBool())
            break;
        if (!tasks_->item(i)->isHidden())
            return i;
    }
    return -1;
}

void Panel::moveTask(QListWidgetItem *item, int destination)
{
    const int original = tasks_->row(item);
    tasks_->insertItem(destination, tasks_->takeItem(original));
    tasks_->setCurrentItem(item);
    saveTasks();
}

void Panel::saveTasks()
{
    QSettings settings(settingsPath_, QSettings::IniFormat);
    settings.setValue(QStringLiteral("projects/names"), projects_);
    settings.setValue(QStringLiteral("projects/folded"), foldedProjects_);
    if (projectColors_.isEmpty())
        settings.remove(QStringLiteral("projects/titleColors"));
    else
        settings.setValue(QStringLiteral("projects/titleColors"), projectColors_);
    settings.remove(QStringLiteral("tasks"));
    settings.beginWriteArray(QStringLiteral("tasks"), tasks_->count() - sections_.size());
    int saved = 0;
    for (int i = 0; i < tasks_->count(); ++i) {
        const auto *item = tasks_->item(i);
        if (item->data(TaskLabels::Section).toBool())
            continue;
        settings.setArrayIndex(saved++);
        settings.setValue(QStringLiteral("title"), item->text());
        settings.setValue(QStringLiteral("done"), item->checkState() == Qt::Checked);
        settings.setValue(QStringLiteral("project"), item->data(TaskLabels::Project).toString());
        settings.setValue(QStringLiteral("priority"), item->data(TaskLabels::Priority).toInt());
        if (item == nextTask_)
            settings.setValue(QStringLiteral("next"), true);
    }
    settings.endArray();
    settings.sync();
    saveError_->setVisible(settings.status() != QSettings::NoError);
    updateSummary();
}

void Panel::updateSummary()
{
    int remaining = 0;
    int visibleRemaining = 0;
    int matchingTasks = 0;
    visibleTaskCount_ = 0;
    QSet<QString> visibleProjects;
    for (int i = 0; i < tasks_->count(); ++i) {
        auto *item = tasks_->item(i);
        if (item->data(TaskLabels::Section).toBool())
            continue;
        const QString project = item->data(TaskLabels::Project).toString();
        const bool inProject = !projectFilter_ || *projectFilter_ == project;
        const bool unfinished = item->checkState() != Qt::Checked;
        matchingTasks += inProject;
        const bool visible = inProject && (!unfinishedOnly_ || unfinished);
        item->setHidden(!visible || foldedProjects_.contains(project));
        remaining += unfinished;
        if (visible) {
            visibleProjects.insert(project);
            ++visibleTaskCount_;
            visibleRemaining += unfinished;
        }
    }
    for (auto it = sections_.cbegin(); it != sections_.cend(); ++it)
        it.value()->setHidden(!visibleProjects.contains(it.key()));
    const QString project = projectFilter_ ? TaskLabels::projectName(*projectFilter_) : QStringLiteral("全部项目");
    const QString label = projectFilter_ ? project : QStringLiteral("全部");
    projectButton_->setText(label);
    projectButton_->setShortcut(QKeySequence());
    projectButton_->setChecked(projectFilter_.has_value());
    projectButton_->setAccessibleName(QStringLiteral("选择项目 · %1").arg(project));
    projectButton_->setToolTip(projectButton_->accessibleName());
    const auto count = QStringLiteral("%1 项未完成").arg(remaining);
    tray_->setToolTip(QStringLiteral("待办 · %1").arg(count));
    const auto badge = remaining > 99 ? QStringLiteral("99+") : QString::number(remaining);
    summary_->setText(visibleRemaining > 99 ? QStringLiteral("99+") : QString::number(visibleRemaining));
    const QString visibleCount = QStringLiteral("%1 · %2 项未完成").arg(project).arg(visibleRemaining);
    summary_->setAccessibleName(visibleCount);
    summary_->setToolTip(visibleCount);
    handleCount_->setText(badge);
    handle_->setAccessibleName(QStringLiteral("展开待办，%1").arg(count));
    handle_->setToolTip(handle_->accessibleName());
    tasks_->setVisible(visibleTaskCount_ > 0);
    emptyState_->setVisible(visibleTaskCount_ == 0);
    if (!visibleTaskCount_) {
        const QString message = unfinishedOnly_ && matchingTasks ? QStringLiteral("已全部完成") : QStringLiteral("暂无待办");
        emptyState_->setText(projectFilter_ ? project + QChar::LineFeed + message : message);
    }
    tasks_->viewport()->update();
    if (windowHandle() && !collapsed_)
        positionPanel();
}
