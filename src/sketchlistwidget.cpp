#include "sketchlistwidget.h"
#include "ui_sketchlistwidget.h"

SketchListWidget::SketchListWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SketchListWidget)
{
    ui->setupUi(this);
}

SketchListWidget::~SketchListWidget()
{
    delete ui;
}

void SketchListWidget::refreshSketches() {
    stk500ListSketches task;
    serial->execute(task);


    for (int i = 0; i < task.sketches.length(); i++) {
        SketchInfo &info = task.sketches[i];

        QListWidgetItem *item = new QListWidgetItem(info.icon, QString(info.name));
        ui->list->addItem(item);
    }
}
