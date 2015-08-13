#include "sketchlistwidget.h"
#include "ui_sketchlistwidget.h"
#include "../imaging/phnimage.h"
#include <QDebug>

SketchListWidget::SketchListWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SketchListWidget)
{
    ui->setupUi(this);
    setStyleSheet("QListWidget::item { border: 5px solid black; }");

    loadIcon = QIcon(":/icons/sketchloading.png");
    defaultIcon = QIcon(":/icons/sketchdefault.png");

    stopLoadingIcons();
}

SketchListWidget::~SketchListWidget()
{
    delete ui;
}

void SketchListWidget::refreshSketches() {
    stk500ListSketches task;
    serial->execute(task);

    // Synchronize the items in the list widget with the sketches
    QListWidgetItem *selected = NULL;
    for (int i = 0; i < task.sketches.length(); i++) {
        SketchInfo sketch = task.sketches[i];
        QString sketchName = sketch.name;

        // First attempt to find the sketch icon in the list
        QListWidgetItem *itemFound = NULL;
        for (int j = i; j < ui->list->count(); j++) {
            QListWidgetItem *item = ui->list->item(j);
            if (item->text() == sketchName) {
                SketchInfo existing = qvariant_cast<SketchInfo>(item->data(Qt::UserRole));
                itemFound = item;
                memcpy(sketch.iconData, existing.iconData, sizeof(sketch.iconData));
                sketch.icon = existing.icon;
                sketch.hasIcon = existing.hasIcon;
                sketch.iconDirty = existing.iconDirty;
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
            sketch.setIcon(SKETCH_DEFAULT_ICON);
            itemFound->setIcon(sketch.icon);
        } else {
            // Schedule for loading
            sketch.iconDirty = true;
            itemFound->setIcon(loadIcon);
        }

        // Check if current sketch
        if (sketchName == task.currentSketch) {
            selected = itemFound;
        }

        // Store sketch information in item
        QVariant var;
        var.setValue(sketch);
        itemFound->setData(Qt::UserRole, var);
    }

    // Remove any items past the count limit
    while (ui->list->count() > task.sketches.length()) {
        delete ui->list->item(ui->list->count() - 1);
    }

    // Select the selected item
    if (selected != NULL) {
        selected->setSelected(true);
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
    QString sketchName = sketch.name;
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

QList<QString> SketchListWidget::getSketchNames() {
    QList<QString> names;
    for (int i = 0; i < ui->list->count(); i++) {
        names.append(ui->list->item(i)->text());
    }
    return names;
}

void SketchListWidget::deleteSketch(const SketchInfo &sketch) {
    if ((sketch.index >= 0) && (sketch.index < ui->list->count())) {
        delete ui->list->item(sketch.index);
    }
}

void SketchListWidget::updateSketch(SketchInfo info) {
    qDebug() << "UPDATE!";
    QListWidgetItem* item;
    if ((info.index < 0) || (info.index >= ui->list->count())) {
        // Add the item
        info.index = ui->list->count();
        item = new QListWidgetItem(info.name);
        ui->list->addItem(item);
        qDebug() << "ADDED.";
    } else {
        // Update existing item
        item = ui->list->item(info.index);
    }
    QVariant var;
    var.setValue(info);
    item->setData(Qt::UserRole, var);
    item->setText(info.name);
    item->setIcon(info.icon);
}

bool SketchListWidget::hasSelectedSketch() {
    return getSelectedName() != "";
}

QString SketchListWidget::getSelectedName() {
    QList<QListWidgetItem*> items = ui->list->selectedItems();
    if (items.isEmpty()) return "";
    return items[0]->text();
}

SketchInfo SketchListWidget::getSelectedSketch() {
    SketchInfo info;
    QList<QListWidgetItem*> items = ui->list->selectedItems();
    if (!items.isEmpty()) {
        info = qvariant_cast<SketchInfo>(items[0]->data(Qt::UserRole));

        // If icon not yet loaded, load it now.
        if (info.iconDirty) {
            stk500LoadIcon task(info);
            serial->execute(task);
            info = task.sketch;
            QVariant var;
            var.setValue(info);
            items[0]->setData(Qt::UserRole, var);
            items[0]->setIcon(info.hasIcon ? info.icon : defaultIcon);
        }
    }
    return info;
}

void SketchListWidget::setSelectedSketch(const SketchInfo &info) {
    QList<QListWidgetItem*> items = ui->list->selectedItems();
    if (!items.isEmpty()) {
        QVariant var;
        var.setValue(info);
        items[0]->setData(Qt::UserRole, var);
        items[0]->setIcon(info.icon);
    }
}

void SketchListWidget::on_list_itemDoubleClicked(QListWidgetItem*) {
    emit sketchDoubleClicked();
}
