#include "chipcontrolwidget.h"
#include "ui_chipcontrolwidget.h"
#include <QMessageBox>

ChipControlWidget::ChipControlWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChipControlWidget)
{
    ui->setupUi(this);

    _active = false;
    lastTask = NULL;
}

ChipControlWidget::~ChipControlWidget()
{
    delete ui;
}

void ChipControlWidget::setActive(bool active) {
    if (!serial || !serial->isOpen()) {
        _active = false;
        return;
    }
    if (_active != active) {
        _active = active;

        // Make sure the task finished handler is connected
        if (active) {
            connect(serial, SIGNAL(taskFinished(stk500Task*)),
                    this, SLOT(serialTaskFinished(stk500Task*)), Qt::UniqueConnection);
        }
    }
    if (active && !serial->isExecuting()) {
        startUpdating();
    }
}

void ChipControlWidget::startUpdating() {
    // Start a new asynchronous task while active
    if (_active) {
        lastTask = new stk500UpdateRegisters(_reg);
        serial->execute(*lastTask, true);
    }
}

void ChipControlWidget::serialTaskFinished(stk500Task *task) {
    // Check if this is our task containing updated registers
    if (task != lastTask) return;
    bool forceItemUpdate = false;

    // Refresh reg variable and delete the task again
    ChipRegisters newReg = lastTask->reg;
    if (!lastTask->isSuccessful()) {
        if (newReg.hasUserChanges() && !lastTask->isCancelled()) {
            QMessageBox::critical(this, "Communication Error",
                                  "An error occurred while updating the device. "
                                  "This can happen when changing registers that "
                                  "control the Serial communication or influence general operation."
                                  "\n\nError: " + lastTask->getErrorMessage());
        }

        newReg = _reg;
        newReg.resetUserChanges();
        forceItemUpdate = true;
    }
    delete lastTask;
    lastTask = NULL;

    // Start updating with the current changes
    startUpdating();
    _reg = newReg;

    QTableWidget *tab = ui->registerTable;
    if (tab->rowCount() == 0) {

        // Set up the column header; exclude first element (is vertical header)
        int columns = CHIPREG_DATA_COLUMNS - 1;
        QStringList headerValues;
        for (int i = 0; i < columns; i++) {
            headerValues.append(_reg.infoHeader().values[i + 1]);
        }
        tab->setColumnCount(columns);
        tab->setHorizontalHeaderLabels(headerValues);
        tab->setSelectionMode(QAbstractItemView::SingleSelection);

        // Fill the table with the initial items
        tab->setRowCount(_reg.count());
        for (int i = 0; i < _reg.count(); i++) {

            // Read the information
            const ChipRegisterInfo &info = _reg.infoByIndex(i);

            // Setup the vertical header item
            tab->setVerticalHeaderItem(i, new QTableWidgetItem(info.address));

            // Create the full row of columns
            for (int col = 0; col < columns; col++) {
                QTableWidgetItem *item = new QTableWidgetItem(info.values[col+1]);
                item->setBackgroundColor(QColor(Qt::white));
                item->setTextAlignment(Qt::AlignCenter);
                if (col != 3) {
                    item->setFlags((item->flags() & ~Qt::ItemIsEditable));
                }
                item->setFlags((item->flags() & ~Qt::ItemIsUserCheckable));
                tab->setItem(i, col, item);
            }
        }

        // Next, refresh all the cells
        forceItemUpdate = true;
    }

    // Update all the cells displayed
    for (int row = 0; row < ui->registerTable->rowCount(); row++) {
        for (int col = 0; col < ui->registerTable->columnCount(); col++) {
            updateItem(ui->registerTable->item(row, col), forceItemUpdate);
        }
    }
}

void ChipControlWidget::updateItem(QTableWidgetItem *item, bool forcedUpdate) {
    // Check if there are any changes or if it is forced to update
    int address = getItemAddress(item);
    bool valueChanged = _reg.changed(address);
    bool refreshNeeded = (valueChanged || forcedUpdate);

    // If it has a bit mask, it is a bit item. Update it as such.
    int bitMask = getItemBitMask(item);
    if (bitMask && refreshNeeded) {
        bool isSet = ((_reg[address] & bitMask) == bitMask);

        // Update background color to reflect state
        item->setBackgroundColor(QColor(isSet ? Qt::yellow : Qt::white));

        // For checkable items, refresh checkbox
        if (item->flags() & Qt::ItemIsUserCheckable) {
            item->setCheckState(isSet ? Qt::Checked : Qt::Unchecked);
        } else {
            item->setData(Qt::CheckStateRole, QVariant(QVariant::Invalid));
        }

        // For items displaying 0/1, update the text
        QString text = item->text();
        if ((text == "0") || (text == "1")) {
            item->setText(isSet ? "1" : "0");
        }
    }

    // Update the displayed text in the value column
    if (item->column() == 3) {
        if (_reg.error(address)) {
            item->setBackgroundColor(QColor(Qt::red));
        } else if (valueChanged) {
            item->setBackgroundColor(QColor(Qt::yellow));
        }
        if (refreshNeeded) {
            item->setText(stk500::getHexText((uint) _reg[address]));
        } else {
            // Fade the value cell to white if it is colored
            QColor c = item->backgroundColor();
            if (c != QColor(Qt::white)) {
                int r = c.red(), g = c.green(), b = c.blue();
                r += 16;
                g += 16;
                b += 16;
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;
                item->setBackgroundColor(QColor::fromRgb(r, g, b));
            }
        }
    }
}

void ChipControlWidget::on_registerTable_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous)
{
    if (previous) {
        previous->setFlags(previous->flags() & ~Qt::ItemIsUserCheckable);
        updateItem(previous, true);
    }
    int currentBitMask = getItemBitMask(current);
    if (currentBitMask) {
        current->setFlags(current->flags() | Qt::ItemIsUserCheckable);
        updateItem(current, true);
    }
}

void ChipControlWidget::on_registerTable_itemChanged(QTableWidgetItem *item)
{
    // Read the bitmask for the item. If there is one, continue
    int bitMask = getItemBitMask(item);
    if (!bitMask) return;

    // Check that the item is checkable
    if (item->data(Qt::CheckStateRole).type() == QVariant::Invalid) return;

    // Toggle the bit if needed
    int address = getItemAddress(item);
    bool bitWasSet = (_reg[address] & bitMask) == bitMask;
    bool itemChecked = (item->checkState() == Qt::Checked);
    if (bitWasSet != itemChecked) {
        _reg[address] ^= bitMask;
    }
}

int ChipControlWidget::getItemBitMask(QTableWidgetItem *item) {
    if (item == NULL) return 0;
    int bitIndex = (11 - item->column());
    return ((bitIndex >= 0) && (bitIndex < 8)) ? (1 << bitIndex) : 0;
}

int ChipControlWidget::getItemAddress(QTableWidgetItem *item) {
    if (item == NULL) return -1;
    return _reg.infoByIndex(item->row()).addressValue;
}

void ChipControlWidget::on_registerTable_itemDoubleClicked(QTableWidgetItem *item)
{
    int bitMask = getItemBitMask(item);
    if (bitMask) {
        _reg[getItemAddress(item)] ^= bitMask;
    }
}
