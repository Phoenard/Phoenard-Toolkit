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

    // Disable the blue outline on OS X
    ui->list->setAttribute(Qt::WA_MacShowFocusRect, false);

    loadIcon = QIcon(":/icons/sketchloading.png");

    stopLoadingIcons();
}

SketchListWidget::~SketchListWidget()
{
    delete ui;
}

void SketchListWidget::setItemSketch(QListWidgetItem *item, const SketchInfo &sketch) {
    QVariant var;
    var.setValue(sketch);
    item->setData(Qt::UserRole, var);
    item->setText(sketch.name);
    item->setIcon(sketch.hasIcon ? sketch.icon : this->loadIcon);
}

SketchInfo SketchListWidget::getItemSketch(const QListWidgetItem *item) {
    return qvariant_cast<SketchInfo>(item->data(Qt::UserRole));
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
                SketchInfo existing = getItemSketch(item);
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
            // Move the item position; make a clone
            QListWidgetItem *itemFoundClone = itemFound->clone();
            delete itemFound;
            itemFound = itemFoundClone;
            ui->list->insertItem(i, itemFound);
        }

        // Reset the item icon loading state
        if (sketch.iconBlock == 0) {
            // No icon available; use default
            sketch.hasIcon = true;
            sketch.iconDirty = false;
            sketch.setIcon((const char*) SKETCH_DEFAULT_ICON);
        } else {
            // Schedule for loading
            sketch.iconDirty = true;
        }

        // Check if current sketch
        if (sketchName == task.currentSketch) {
            selected = itemFound;
        }

        // Store sketch information in item
        setItemSketch(itemFound, sketch);
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
        SketchInfo itemSketch = getItemSketch(item);
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
            setItemSketch(item, sketch);
            break;
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

void SketchListWidget::updateSketch(SketchInfo sketch) {
    QListWidgetItem* item;
    if ((sketch.index < 0) || (sketch.index >= ui->list->count())) {
        // Add the item
        sketch.index = ui->list->count();
        item = new QListWidgetItem(sketch.name);
        ui->list->addItem(item);
    } else {
        // Update existing item
        item = ui->list->item(sketch.index);
    }
    setItemSketch(item, sketch);
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
    SketchInfo sketch;
    QList<QListWidgetItem*> items = ui->list->selectedItems();
    if (!items.isEmpty()) {
        sketch = getItemSketch(items[0]);

        // If icon not yet loaded, load it now.
        if (sketch.iconDirty) {
            stk500LoadIcon task(sketch);
            serial->execute(task);
            sketch = task.sketch;
            setItemSketch(items[0], sketch);
        }
    }
    return sketch;
}

void SketchListWidget::setSelectedSketch(const SketchInfo &sketch) {
    QList<QListWidgetItem*> items = ui->list->selectedItems();
    if (!items.isEmpty()) {
        setItemSketch(items[0], sketch);
    }
}

void SketchListWidget::on_list_itemDoubleClicked(QListWidgetItem*) {
    emit sketchDoubleClicked();
}
