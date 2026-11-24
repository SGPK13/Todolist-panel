#pragma once

#include <QColor>
#include <QIcon>
#include <QString>
#include <QVariant>
#include <Qt>

namespace TaskLabels {
enum Role { Project = Qt::UserRole + 1, Priority, Section, Next, SavedDone, ProjectColorIndex, ProjectColorChoice };
constexpr int priorityCount = 3;
int validPriority(int value);
QString priorityName(int value);
QColor priorityColor(int value, bool dark);
QString projectName(const QString &project);
QColor projectTitleColor(int automaticIndex, const QVariant &choice, bool dark);
}
