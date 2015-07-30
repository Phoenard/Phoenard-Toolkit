#ifndef SKETCHLISTWIDGET_H
#define SKETCHLISTWIDGET_H

#include <QWidget>
#include "mainmenutab.h"

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

private:
    Ui::SketchListWidget *ui;
};

#endif // SKETCHLISTWIDGET_H
