#include "sketchlistwidget.h"
#include "ui_sketchlistwidget.h"

SketchListWidget::SketchListWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SketchListWidget)
{
    ui->setupUi(this);

    setStyleSheet("QListWidget::item { border: 5px solid black; }");
}

SketchListWidget::~SketchListWidget()
{
    delete ui;
}

void SketchListWidget::refreshSketches() {
    stk500ListSketches task;
    serial->execute(task);

    ui->list->clear();
    QListWidgetItem *selItem = NULL;
    for (int i = 0; i < task.sketches.length(); i++) {
        SketchInfo &info = task.sketches[i];

        QListWidgetItem *item = new QListWidgetItem(info.icon, QString(info.name));
        if (item->text() == task.currentSketch) {
            selItem = item;
        }
        ui->list->addItem(item);
    }
    if (selItem != NULL) {
        selItem->setSelected(true);
    }
}

bool SketchListWidget::hasSelectedSketch() {
    return getSelectedSketch() != "";
}

QString SketchListWidget::getSelectedSketch() {
    QList<QListWidgetItem*> items = ui->list->selectedItems();
    if (items.isEmpty()) return "";
    return items[0]->text();
}

void SketchListWidget::on_list_itemDoubleClicked(QListWidgetItem*) {
    emit sketchDoubleClicked();
}
