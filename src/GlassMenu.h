#pragma once

#include <QMenu>

class GlassMenu final : public QMenu
{
public:
    GlassMenu(QWidget *parent, QWidget *panelContent, bool dark, int transparency);
    void setTransparency(int percent);
    void setActionTextColor(QAction *action, const QColor &color);

protected:
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void clipBackdrop();
    bool dark_;
    int transparency_;
    bool blurred_ = false;
};
