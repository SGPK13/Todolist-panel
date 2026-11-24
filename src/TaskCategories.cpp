#include "Panel.h"
#include "GlassMenu.h"
#include "ProjectColorPicker.h"
#include "TaskLabels.h"

#include <QGuiApplication>
#include <QInputMethod>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QWidgetAction>
#include <QWindow>
#include <iterator>

namespace {
struct ProjectTint { const char *name; QRgb light; QRgb dark; };
constexpr ProjectTint projectTints[] = {
    {"雾蓝", 0xff255675, 0xffa7cde5}, {"苔绿", 0xff325e45, 0xffa8d2b7},
    {"灰紫", 0xff624b79, 0xffcdb8e3}, {"栗棕", 0xff764e37, 0xffe4bda5},
    {"青绿", 0xff255d60, 0xffa0d4d3}, {"豆沙", 0xff73485e, 0xffdeb5ca}
};
}

namespace TaskLabels {
QColor projectTitleColor(int automaticIndex, const QVariant &choice, bool dark)
{
    int index = automaticIndex;
    if (choice.typeId() == QMetaType::QString) {
        const QColor custom(choice.toString());
        if (custom.isValid())
            return custom;
    } else if (choice.typeId() == QMetaType::Int && choice.toInt() >= 0
               && choice.toInt() < int(std::size(projectTints))) {
        index = choice.toInt();
    }
    if (index < 0)
        return dark ? QColor("#d2dce3") : QColor("#303438");
    const auto &tint = projectTints[index % std::size(projectTints)];
    return QColor::fromRgb(dark ? tint.dark : tint.light);
}

int validPriority(int value)
{
    return value >= 0 && value < priorityCount ? value : 0;
}

QString priorityName(int value)
{
    static const QString names[]{QStringLiteral("普通"), QStringLiteral("优先"), QStringLiteral("紧急")};
    return names[validPriority(value)];
}

QColor priorityColor(int value, bool dark)
{
    static const QRgb light[]{0xff708390, 0xffb36b1b, 0xffbf444d};
    static const QRgb night[]{0xffa1b5c2, 0xffefb660, 0xfffb9297};
    return QColor::fromRgb((dark ? night : light)[validPriority(value)]);
}

QString projectName(const QString &project)
{
    if (project.isEmpty())
        return QStringLiteral("未分类");
    if (project == QStringLiteral("未分类") || project == QStringLiteral("全部项目"))
        return QStringLiteral("「%1」").arg(project);
    return project;
}
}

namespace {
class ProjectNameEdit final : public QLineEdit
{
public:
    using QLineEdit::QLineEdit;

protected:
    void focusInEvent(QFocusEvent *event) override
    {
        QLineEdit::focusInEvent(event);
        // QMenu may assign widget focus before its popup HWND is shown.
        QTimer::singleShot(0, this, &ProjectNameEdit::syncInputMethodFocus);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        QLineEdit::mousePressEvent(event);
        syncInputMethodFocus();
    }

private:
    void syncInputMethodFocus()
    {
#ifdef Q_OS_WIN
        if (!hasFocus() || !isVisible())
            return;
        // QWidgetAction routes ordinary keys to the editor even while the
        // owner HWND still has native focus. IME composition needs the popup.
        auto *popup = window()->windowHandle();
        if (popup && QGuiApplication::focusWindow() != popup)
            popup->requestActivate();
        QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
#endif
    }
};

QIcon dotIcon(QColor color, bool dark, bool showChevron = false)
{
    QPixmap pixmap(showChevron ? 42 : 32, 32);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(showChevron ? QRectF(2, 4.5, 7, 7) : QRectF(3.5, 3.5, 9, 9));
    if (showChevron) {
        painter.setPen(QPen(dark ? QColor("#c5cfd4") : QColor("#52616a"),
                            1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF chevron[]{QPointF(12, 7), QPointF(15, 10), QPointF(18, 7)};
        painter.drawPolyline(chevron, 3);
    }
    return QIcon(pixmap);
}
}

void Panel::updateDraftPriority()
{
    priorityButton_->setIcon(dotIcon(TaskLabels::priorityColor(draftPriority_, dark_), dark_, true));
    const QString description = QStringLiteral("新待办的紧急程度 · %1").arg(TaskLabels::priorityName(draftPriority_));
    priorityButton_->setToolTip(description + QStringLiteral("\n输入时按 Alt+1 普通、Alt+2 优先、Alt+3 紧急"));
    priorityButton_->setAccessibleName(description);
}

void Panel::populatePriorityMenu(QMenu &menu, QListWidgetItem *item)
{
    const int current = item ? item->data(TaskLabels::Priority).toInt() : draftPriority_;
    for (int level = 0; level < TaskLabels::priorityCount; ++level) {
        auto *action = menu.addAction(dotIcon(TaskLabels::priorityColor(level, dark_), dark_), TaskLabels::priorityName(level));
        action->setCheckable(true);
        action->setChecked(level == current);
        connect(action, &QAction::triggered, this, [this, item, level] {
            if (item) {
                item->setData(TaskLabels::Priority, level);
            } else {
                draftPriority_ = level;
                updateDraftPriority();
                input_->setFocus();
            }
        });
    }
}

void Panel::showProjectColorMenu(const QString &project, const QPoint &globalPosition)
{
    const auto current = projectColors_.value(project);
    const int index = projects_.indexOf(project);
    GlassMenu menu(this, expanded_, dark_, transparency_);
    menu.addSection(QStringLiteral("项目颜色"));
    auto *automatic = menu.addAction(QStringLiteral("自动配色"));
    automatic->setCheckable(true);
    automatic->setChecked(!current.isValid());
    for (int i = 0; i < int(std::size(projectTints)); ++i) {
        const QColor tint = TaskLabels::projectTitleColor(index, i, dark_);
        auto *action = menu.addAction(dotIcon(tint, dark_), QString::fromUtf8(projectTints[i].name));
        action->setData(i);
        action->setCheckable(true);
        action->setChecked(current.typeId() == QMetaType::Int && current.toInt() == i);
    }
    auto *custom = menu.addAction(QStringLiteral("自定义颜色…"));
    custom->setCheckable(true);
    custom->setChecked(current.typeId() == QMetaType::QString);
    addPanelSettingsAction(menu, globalPosition);
    auto *selection = menu.exec(globalPosition);
    if (!selection)
        return;
    if (selection == custom) {
        GlassMenu picker(this, expanded_, dark_, transparency_);
        const QRect heading = tasks_->visualItemRect(sections_.value(project));
        const QPoint anchor = tasks_->viewport()->mapToGlobal(heading.bottomLeft() + QPoint(0, 3));
        const QColor color = pickProjectColor(picker, anchor, TaskLabels::projectName(project),
                                               TaskLabels::projectTitleColor(index, current, dark_), dark_);
        if (!color.isValid())
            return;
        projectColors_.insert(project, color.name());
    } else if (selection == automatic) {
        projectColors_.remove(project);
    } else if (selection->data().isValid()) {
        projectColors_.insert(project, selection->data());
    } else {
        return;
    }
    sections_.value(project)->setData(TaskLabels::ProjectColorChoice, projectColors_.value(project));
    saveTasks();
}

void Panel::selectProject(std::optional<QString> project)
{
    projectFilter_ = project;
    draftProject_.reset();
    updateDraftTarget();
    QSettings settings(settingsPath_, QSettings::IniFormat);
    if (project)
        settings.setValue(QStringLiteral("panel/projectFilter"), *project);
    else
        settings.remove(QStringLiteral("panel/projectFilter"));
    updateSummary();
    tasks_->scrollToTop();
}

void Panel::populateProjectMenu(GlassMenu &menu, QListWidgetItem *item)
{
    const auto choose = [this, item](const QString &project) {
        if (item)
            assignProject(item, project);
        else
            selectProject(project);
    };
    if (!item) {
        auto *all = menu.addAction(QStringLiteral("全部项目"));
        all->setCheckable(true);
        all->setChecked(!projectFilter_);
        connect(all, &QAction::triggered, this, [this] { selectProject(std::nullopt); });
    }
    QStringList choices = projects_;
    choices.prepend(QString());
    for (const QString &project : choices) {
        auto *action = menu.addAction(TaskLabels::projectName(project));
        const auto color = TaskLabels::projectTitleColor(projects_.indexOf(project), projectColors_.value(project), dark_);
        menu.setActionTextColor(action, color);
        action->setCheckable(true);
        action->setChecked(item ? item->data(TaskLabels::Project).toString() == project : projectFilter_ == project);
        connect(action, &QAction::triggered, this, [choose, project] { choose(project); });
    }
    menu.addSeparator();
    auto *name = new ProjectNameEdit(&menu);
    name->setObjectName(QStringLiteral("newProjectName"));
    name->setMaxLength(40);
    name->setMinimumWidth(160);
    name->setFixedHeight(30);
    name->setPlaceholderText(QStringLiteral("新项目名称…"));
    name->setAccessibleName(QStringLiteral("新项目名称，按回车确认"));
    auto *entry = new QWidgetAction(&menu);
    entry->setDefaultWidget(name);
    menu.addAction(entry);
    connect(name, &QLineEdit::returnPressed, &menu, [this, name, choose, &menu] {
        const QString project = name->text().trimmed();
        if (project.isEmpty())
            return;
        if (!projects_.contains(project)) {
            projects_.append(project);
            QSettings settings(settingsPath_, QSettings::IniFormat);
            settings.setValue(QStringLiteral("projects/names"), projects_);
        }
        choose(project);
        for (QMenu *open = &menu; open; open = qobject_cast<QMenu *>(open->parentWidget()))
            open->close();
    });
}
