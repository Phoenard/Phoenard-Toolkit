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

    // Refresh local reg variable and delete the task again
    _reg = lastTask->reg;
    delete lastTask;
    lastTask = NULL;

    QTableWidget *tab = ui->registerTable;
    if (tab->rowCount() == 0) {
        // Set up the column count and column header
        tab->setColumnCount(10);
        tab->setHorizontalHeaderLabels(QStringList() << "Address" << "Value"
                                       << "b0" << "b1" << "b2" << "b3"
                                       << "b4" << "b5" << "b6" << "b7");

        // Fill the table with the initial items
        tab->setRowCount(_reg.count());
        for (int i = 0; i < _reg.count(); i++) {

            // Setup the vertical header item
            tab->setVerticalHeaderItem(i, new QTableWidgetItem(_reg.name(i)));

            // Create the full row of columns
            for (int col = 0; col < 10; col++) {
                QTableWidgetItem *item = new QTableWidgetItem("");
                if (col == 0) item->setText(stk500::getHexText(_reg.addr(i)));
                item->setBackgroundColor(QColor(Qt::white));
                item->setTextAlignment(Qt::AlignCenter);
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

    // Execute the next update
    startUpdating();
}

void ChipControlWidget::updateEntry(int index, bool forceUpdate) {
    QTableWidget *tab = ui->registerTable;
    bool valueChanged = (!forceUpdate && _reg.changed(index));
    if (valueChanged || forceUpdate) {
        if (!forceUpdate) {
            tab->item(index, 1)->setBackgroundColor(QColor(Qt::yellow));
        }

        uint value = (uint) _reg[index];
        tab->item(index, 1)->setText(stk500::getHexText(value));

        // Update the bit values
        for (int b = 0; b < 8; b++) {
            bool isOn = ((value & (1 << b)) != 0);
            QTableWidgetItem *item = tab->item(index, 2 + b);
            item->setBackgroundColor(QColor(isOn ? Qt::yellow : Qt::white));
            item->setText(isOn ? "1" : "0");
        }
    } else {
        QColor c = tab->item(index, 1)->backgroundColor();
        if (c != QColor(Qt::white)) {
            int r = c.red(), g = c.green(), b = c.blue();
            r += 8;
            g += 8;
            b += 8;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            tab->item(index, 1)->setBackgroundColor(QColor::fromRgb(r, g, b));
        }
    }
}
