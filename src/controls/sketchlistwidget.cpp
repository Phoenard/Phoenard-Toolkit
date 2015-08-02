#include "sketchlistwidget.h"
#include "ui_sketchlistwidget.h"
#include <QDebug>

SketchListWidget::SketchListWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SketchListWidget)
{
    ui->setupUi(this);
    setStyleSheet("QListWidget::item { border: 5px solid black; }");

    stopLoadingIcons();
}

SketchListWidget::~SketchListWidget()
{
    delete ui;
}

void SketchListWidget::refreshSketches() {
    stk500ListSketches task;
    serial->execute(task);

    // Default icon to use
    QIcon defaultIcon(":/icons/sketchdefault.png");
    QIcon loadIcon(":/icons/sketchloading.png");

    // Synchronize the items in the list widget with the sketches
    for (int i = 0; i < task.sketches.length(); i++) {
        SketchInfo sketch = task.sketches[i];
        QString sketchName = QString(sketch.name);

        // First attempt to find the sketch icon in the list
        QListWidgetItem *itemFound = NULL;
        for (int j = i; j < ui->list->count(); j++) {
            QListWidgetItem *item = ui->list->item(j);
            if (item->text() == sketchName) {
                itemFound = item;
                sketch = qvariant_cast<SketchInfo>(item->data(Qt::UserRole));
                break;
            }
        }

        // Add a new item at the current position if not found
        if (itemFound == NULL) {
            itemFound = new QListWidgetItem(sketchName);
            ui->list->insertItem(i, itemFound);

        } else if (itemFound != ui->list->item(i)) {
            // Move the item position
            ui->list->removeItemWidget(itemFound);
            ui->list->addItem(itemFound);
        }

        // Reset the item icon loading state
        if (sketch.hasIcon) {
            // Use previous icon but reload it
            sketch.iconDirty = true;
            itemFound->setIcon(sketch.icon);
        } else if (sketch.iconBlock == 0) {
            // No icon available; use default
            sketch.hasIcon = true;
            sketch.iconDirty = false;
            itemFound->setIcon(defaultIcon);
        } else {
            // Schedule for loading
            sketch.iconDirty = true;
            itemFound->setIcon(loadIcon);
        }

        // Store sketch information in item
        QVariant var;
        var.setValue(sketch);
        itemFound->setData(Qt::UserRole, var);
    }

    // Remove any items past the count limit
    while (ui->list->count() > task.sketches.length()) {
        ui->list->removeItemWidget(ui->list->item(ui->list->count() - 1));
    }

    // Connect the signal fired when a task is finished
    connect(serial, SIGNAL(taskFinished(stk500Task*)),
            this, SLOT(serialTaskFinished(stk500Task*)), Qt::UniqueConnection);

    // Initiate icon load processing
    startLoadingIcons();
}

void SketchListWidget::stopLoadingIcons() {
    lastTask = NULL;
}

void SketchListWidget::startLoadingIcons() {
    // Find the first icon that requires loading
    SketchInfo sketch;
    bool foundSketch = false;
    for (int i = 0; i < ui->list->count(); i++) {
        QListWidgetItem *item = ui->list->item(i);
        SketchInfo itemSketch = qvariant_cast<SketchInfo>(item->data(Qt::UserRole));
        if (itemSketch.iconDirty) {
            if (itemSketch.hasIcon) {
                // Low priority, take first
                if (!foundSketch) sketch = itemSketch;
                foundSketch = true;
            } else {
                // High priority, take it now
                sketch = itemSketch;
                foundSketch = true;
                break;
            }
        }
    }
    if (foundSketch) {
        lastTask = new stk500LoadIcon(sketch);
        serial->execute(*lastTask, true);
    } else {
        stopLoadingIcons();
    }
}

void SketchListWidget::serialTaskFinished(stk500Task *task) {
    // Filter out the requested task
    if (lastTask != task) return;

    // Find the item belonging to this task
    SketchInfo &sketch = lastTask->sketch;
    QString sketchName = QString(sketch.name);
    for (int i = 0; i < ui->list->count(); i++) {
        QListWidgetItem *item = ui->list->item(i);
        if (item->text() == sketchName) {
            item->setIcon(sketch.icon);

            QVariant var;
            var.setValue(sketch);
            item->setData(Qt::UserRole, var);
        }
    }

    // Memory management: need to delete this when done
    delete lastTask;

    // If not aborted, continue with the next task
    startLoadingIcons();
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
