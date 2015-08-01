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
    QString getSelectedSketch();
    void startLoadingIcons();

signals:
    void sketchDoubleClicked();

private slots:
    void serialTaskFinished(stk500Task *task);
    void on_list_itemDoubleClicked(QListWidgetItem *item);

private:
    Ui::SketchListWidget *ui;
};

#endif // SKETCHLISTWIDGET_H
