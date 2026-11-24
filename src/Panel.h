#pragma once

#include <QPixmap>
#include <QHash>
#include <QPersistentModelIndex>
#include <QTimer>
#include <QStringList>
#include <QVariantAnimation>
#include <QVariantMap>
#include <QWidget>

#include <memory>
#include <optional>

class GlassShadow;
class GlassMenu;
class QCloseEvent;
class QContextMenuEvent;
class QHideEvent;
class QLabel;
class QLineEdit;
class QMenu;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QScreen;
class QSettings;
class QShowEvent;
class QSystemTrayIcon;

class Panel : public QWidget
{
public:
    explicit Panel(QSettings &settings);
    ~Panel() override;

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void closeEvent(QCloseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void applyAppearance();
    void setTransparency(int percent);
    void addTransparencyControl(QMenu *menu);
    QSize expandedSize(const QRect &workArea) const;
    void watchScreen(QScreen *screen);
    QScreen *targetScreen() const;
    void positionPanel();
    bool panelIsShowing() const;
    void schedulePanelRecovery();
    void restorePanel();
    void registerSessionNotifications();
    void unregisterSessionNotifications();
    void setCollapsed(bool collapsed);
    void finishTransition(bool restoreFocus = true);
    void showPanelMenu(QWidget *anchor, std::optional<QPoint> position = std::nullopt);
    void addPanelSettingsAction(QMenu &menu, const QPoint &globalPosition);
    void showTaskMenu(const QPoint &point);
    void showProjectColorMenu(const QString &project, const QPoint &globalPosition);
    void addTask(bool continueEntry = false);
    void setComposerOpen(bool open);
    QListWidgetItem *appendTask(const QString &title, bool done, const QString &project, int priority);
    void insertInProject(QListWidgetItem *item, int groupOffset = -1);
    void setNextTask(QListWidgetItem *item);
    void deleteTask(QListWidgetItem *item);
    void undoLastChange();
    void clearUndo();
    void offerUndo(const QString &message);
    void updateDraftTarget();
    void assignProject(QListWidgetItem *item, const QString &project);
    void populateProjectMenu(GlassMenu &menu, QListWidgetItem *item = nullptr);
    void populatePriorityMenu(QMenu &menu, QListWidgetItem *item = nullptr);
    void selectProject(std::optional<QString> project);
    void updateDraftPriority();
    int adjacentVisibleRow(QListWidgetItem *item, int direction) const;
    void updateTask(QListWidgetItem *item);
    void moveTask(QListWidgetItem *item, int destination);
    void saveTasks();
    void updateSummary();

    const QString settingsPath_;
    QWidget *expanded_;
    QPushButton *handle_;
    QLabel *summary_;
    QLabel *handleArrow_;
    QLabel *handleCount_;
    QLabel *emptyState_;
    QLabel *saveError_;
    QLineEdit *input_;
    QWidget *composer_ = nullptr;
    QPushButton *quickAdd_ = nullptr;
    QPushButton *undoButton_ = nullptr;
    QTimer *undoTimer_ = nullptr;
    QLabel *draftTarget_ = nullptr;
    QListWidget *tasks_;
    QPersistentModelIndex pressedPriority_;
    QPushButton *collapseButton_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QPushButton *projectButton_ = nullptr;
    QPushButton *priorityButton_ = nullptr;
    QStringList projects_;
    QStringList foldedProjects_;
    QVariantMap projectColors_;
    QHash<QString, QListWidgetItem *> sections_;
    std::optional<QString> projectFilter_;
    std::optional<QString> draftProject_;
    QListWidgetItem *nextTask_ = nullptr;
    QListWidgetItem *completedUndo_ = nullptr;
    std::unique_ptr<QListWidgetItem> deletedUndo_;
    int deletedGroupOffset_ = 0;
    bool undoWasNext_ = false;
    int draftPriority_ = 0;
    int visibleTaskCount_ = 0;
    QStringList preferredScreen_;
    QString appearance_;
    QString motion_;
    int transparency_ = 51;
    QSystemTrayIcon *tray_;
    std::unique_ptr<GlassShadow> shadow_;
    QTimer recoveryTimer_;
    QTimer visibilityTimer_;
    WId sessionWindow_ = 0;
    QVariantAnimation transition_;
    QPixmap expandedSnapshot_;
    QPixmap handleSnapshot_;
    qreal openness_ = 1;
    bool collapsed_ = false;
    bool dark_ = false;
    bool unfinishedOnly_ = false;
};
