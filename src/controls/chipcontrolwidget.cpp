#include "chipcontrolwidget.h"
#include "ui_chipcontrolwidget.h"

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

void ChipControlWidget::on_registerTable_cellDoubleClicked(int row, int column)
{
    int address = _reg.infoByIndex(row).addressValue;
    int bitIndex = (11 - column);
    if ((bitIndex >= 0) && (bitIndex < 8) && (address != -1)) {
        _reg[address] ^= (1 << bitIndex);
        qDebug() << "NEW VALUE: " << stk500::getHexText(_reg[address]);
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

    // Refresh reg variable and delete the task again
    ChipRegisters newReg = lastTask->reg;
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
                    item->setFlags(item->flags() ^ Qt::ItemIsEditable);
                }
                tab->setItem(i, col, item);
            }

            // Update the entry
            updateEntry(i, true);
        }
    } else {
        // Update all the table entries that changed value
        for (int i = 0; i < _reg.count(); i++) {
            updateEntry(i, false);
        }
    }
}

void ChipControlWidget::updateEntry(int index, bool forceUpdate) {
    int address = _reg.infoByIndex(index).addressValue;
    bool valueChanged = (!forceUpdate && _reg.changed(address));
    QTableWidgetItem* valueItem = ui->registerTable->item(index, 3);

    // Highlight the value cell when value changes; fades out
    if (valueChanged) {
        valueItem->setBackgroundColor(QColor(Qt::yellow));
    }

    // Refresh the bit fields and value text
    if (valueChanged || forceUpdate) {
        uint value = (uint) _reg[address];
        valueItem->setText(stk500::getHexText(value));

        // Update the bit values
        for (int b = 0; b < 8; b++) {
            bool isOn = ((value & (1 << b)) != 0);
            QTableWidgetItem *item = ui->registerTable->item(index, 11-b);
            item->setBackgroundColor(QColor(isOn ? Qt::yellow : Qt::white));

            QString text = item->text();
            if ((text == "0") || (text == "1")) {
                item->setText(isOn ? "1" : "0");
            }
        }
    } else {
        // Fade the value cell to white if it is colored
        QColor c = valueItem->backgroundColor();
        if (c != QColor(Qt::white)) {
            int r = c.red(), g = c.green(), b = c.blue();
            r += 8;
            g += 8;
            b += 8;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            valueItem->setBackgroundColor(QColor::fromRgb(r, g, b));
        }
    }
}
