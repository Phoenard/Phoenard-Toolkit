#ifndef SKETCHLISTWIDGET_H
#define SKETCHLISTWIDGET_H

#include <QWidget>
#include "../mainmenutab.h"
#include <QListWidgetItem>

namespace Ui {
class SketchListWidget;
}

class SketchListWidget : public QWidget, public MainMenuTab
{
    Q_OBJECT

public:
    explicit SketchListWidget(QWidget *parent = 0);
    ~SketchListWidget();
    void refreshSketches();
    bool hasSelectedSketch();
    QString getSelectedName();
    SketchInfo getSelectedSketch();
    void setSelectedSketch(const SketchInfo &info);
    void startLoadingIcons();
    void stopLoadingIcons();

signals:
    void sketchDoubleClicked();

private slots:
    void serialTaskFinished(stk500Task *task);
    void on_list_itemDoubleClicked(QListWidgetItem *item);

private:
    Ui::SketchListWidget *ui;
    stk500LoadIcon *lastTask;
    QIcon loadIcon;
    QIcon defaultIcon;
};

#endif // SKETCHLISTWIDGET_H
