#include "chipcontrolwidget.h"
#include "ui_chipcontrolwidget.h"

#define PINMAP_COL_READ   4
#define PINMAP_COL_MODE   5
#define PINMAP_COL_WRITE  6
#define PINMAP_COL_ADC    7

ChipControlWidget::ChipControlWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChipControlWidget)
{
    ui->setupUi(this);

    _active = false;
    _ignoreChanges = false;
    _forceRefresh = false;
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

void ChipControlWidget::setDisplayMode(int mode) {
    ui->stackedWidget->setCurrentIndex(mode);
    _forceRefresh = true;
}

bool ChipControlWidget::showRegisters() {
    return (ui->stackedWidget->currentWidget() == ui->registerPage);
}

void ChipControlWidget::startUpdating() {
    // Start a new asynchronous task while active
    if (_active) {
        lastTask = new stk500UpdateRegisters(_reg);
        lastTask->readADC = !showRegisters();
        serial->execute(*lastTask, true);
        _reg.resetUserChanges();
    }
}

void ChipControlWidget::serialTaskFinished(stk500Task *task) {
    // Check if this is our task containing updated registers
    if (task != lastTask) return;
    bool forceItemUpdate = _forceRefresh;
    _forceRefresh = false;

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

    // Refresh the registers but keep user changes that happened while updating
    newReg.applyUserChanges(_reg);
    _reg = newReg;

    // Start updating with the current changes
    startUpdating();

    if (ui->stackedWidget->currentWidget() == ui->registerPage) {
        QTableWidget *tab = ui->registerTable;
        if (tab->rowCount() == 0) {

            // While initializing the table, do not fire events
            _ignoreChanges = true;

            // Set up the column header; exclude first element (is vertical header)
            const int columns = CHIPREG_DATA_COLUMNS - 1;
            const int header_padding = 12;
            const ChipRegisterInfo &headerInfo = _reg.infoHeader();
            int* columnWidths = new int[columns];
            QFontMetrics fontMetrics(tab->font());

            // Go by all columns and refresh the header values
            tab->setSelectionMode(QAbstractItemView::SingleSelection);
            tab->setColumnCount(columns);
            for (int col = 0; col < columns; col++) {
                QString colHeader = headerInfo.values[col + 1];
                columnWidths[col] = fontMetrics.width(colHeader) + header_padding;
                tab->setHorizontalHeaderItem(col, new QTableWidgetItem(colHeader));
            }

            // Fill the table with the initial items
            tab->setRowCount(_reg.count());
            int moduleRowStart = 0;
            for (int row = 0; row < _reg.count(); row++) {

                // Read the information
                const ChipRegisterInfo &info = _reg.infoByIndex(row);

                // Setup the vertical header item
                tab->setVerticalHeaderItem(row, new QTableWidgetItem(info.address));

                // Create the full row of columns
                for (int col = 0; col < columns; col++) {

                    // Update and add the cell item
                    QTableWidgetItem *item = new QTableWidgetItem(info.values[col+1]);
                    item->setBackgroundColor(QColor(Qt::white));
                    if (col != 3) {
                        item->setFlags((item->flags() & ~Qt::ItemIsEditable));
                    }
                    if (col >= 2) {
                        item->setTextAlignment(Qt::AlignCenter);
                    }
                    item->setFlags((item->flags() & ~Qt::ItemIsUserCheckable));
                    tab->setItem(row, col, item);

                    // Increase the column width as needed
                    int colWidth = fontMetrics.width(item->text()) + header_padding;
                    if (columnWidths[col] < colWidth) {
                        columnWidths[col] = colWidth;
                    }
                }

                // If module name is the same as the row above; merge
                if (row > 0 && (_reg.infoByIndex(moduleRowStart).module == info.module) && (info.module != "-")) {
                    tab->setSpan(moduleRowStart, 0, row-moduleRowStart+1, 1);
                } else {
                    moduleRowStart = row;
                }
            }

            // Apply column widths
            for (int col = 0; col < columns; col++) {
                tab->setColumnWidth(col, columnWidths[col]);
            }
            delete[] columnWidths;

            // Next, refresh all the cells
            forceItemUpdate = true;

            // Done.
            _ignoreChanges = false;
        }

        // Update all the cells displayed
        for (int row = 0; row < ui->registerTable->rowCount(); row++) {
            for (int col = 0; col < ui->registerTable->columnCount(); col++) {
                updateItem(ui->registerTable->item(row, col), forceItemUpdate);
            }
        }

    } else if (ui->stackedWidget->currentWidget() == ui->pinmapPage) {

        // Initialize pinmap table
        QTableWidget *tab = ui->pinmapTable;
        if (tab->rowCount() == 0) {
            const PinMapInfo &pinmapHeader = _reg.pinmapHeader();
            const QList<PinMapInfo> &pinmap = _reg.pinmap();

            // Set up the column header; exclude first element (is vertical header)
            const int columns = 8;
            const int header_padding = 15;
            int moduleRowStart = 0;
            int* columnWidths = new int[columns];
            QFontMetrics fontMetrics(tab->font());

            // Go by all columns and refresh the header values
            tab->setSelectionMode(QAbstractItemView::SingleSelection);
            tab->setColumnCount(columns);
            for (int col = 0; col < columns; col++) {
                QString colHeader;
                if (col == PINMAP_COL_READ) {
                    colHeader = "Read";
                    columnWidths[col] = 70;
                } else if (col == PINMAP_COL_MODE) {
                    colHeader = "Mode";
                    columnWidths[col] = 70;
                } else if (col == PINMAP_COL_WRITE) {
                    colHeader = "Write";
                    columnWidths[col] = 70;
                } else if (col == PINMAP_COL_ADC) {
                    colHeader = "ADC";
                    columnWidths[col] = 90;
                } else {
                    colHeader = pinmapHeader.values[col + 1];
                    columnWidths[col] = fontMetrics.width(colHeader) + header_padding;
                }
                tab->setHorizontalHeaderItem(col, new QTableWidgetItem(colHeader));
            }

            // Fill the table with the initial items
            tab->setRowCount(pinmap.count());
            for (int row = 0; row < tab->rowCount(); row++) {

                // Read the information
                const PinMapInfo &info = pinmap[row];

                // Setup the vertical header item
                tab->setVerticalHeaderItem(row, new QTableWidgetItem(info.values[0]));

                // Create the full row of columns
                for (int col = 0; col < columns; col++) {

                    QString itemText;
                    if (col == PINMAP_COL_ADC) {
                        itemText = "";
                    } else if (col >= PINMAP_COL_READ) {
                        itemText = "Value";
                    } else {
                        itemText = info.values[col+1];
                    }

                    // Update and add the cell item
                    QTableWidgetItem *item = new QTableWidgetItem(itemText);
                    item->setBackgroundColor(QColor(Qt::white));
                    item->setFlags((item->flags() & ~Qt::ItemIsEditable));
                    if (col >= 2) {
                        item->setTextAlignment(Qt::AlignCenter);
                    }
                    tab->setItem(row, col, item);

                    // Increase the column width as needed
                    int colWidth = fontMetrics.width(item->text()) + header_padding;
                    if (columnWidths[col] < colWidth) {
                        columnWidths[col] = colWidth;
                    }
                }

                // If module name is the same as the row above; merge
                if (row > 0 && (pinmap[moduleRowStart].module == info.module)) {
                    tab->setSpan(moduleRowStart, 0, row-moduleRowStart+1, 1);
                } else {
                    moduleRowStart = row;
                }
            }

            // Apply column widths
            for (int col = 0; col < columns; col++) {
                tab->setColumnWidth(col, columnWidths[col]);
            }
            delete[] columnWidths;

            // Refresh all rows next time
            forceItemUpdate = true;
        }

        // Update all rows
        for (int row = 0; row < tab->rowCount(); row++) {
            updatePinRow(row, forceItemUpdate);
        }
    }
}

void ChipControlWidget::updatePinRow(int row, bool forcedUpdate) {
    // Refresh the states using the PIN, DDR and PORT addresses
    const PinMapInfo &info = _reg.pinmap()[row];
    int mask = info.addr_mask;

    // Refresh the input state cell by reading PIN
    if (forcedUpdate || _reg.changed(info.addr_pin)) {
        bool isSet = ((_reg[info.addr_pin] & mask) == mask);

        QTableWidgetItem *item = ui->pinmapTable->item(row, PINMAP_COL_READ);
        item->setText(isSet ? "High" : "Low");
        item->setBackgroundColor(QColor(isSet ? Qt::green : Qt::red));
    }

    // Refresh the mode cell by reading DDR
    if (forcedUpdate || _reg.changed(info.addr_ddr)) {
        bool isSet = ((_reg[info.addr_ddr] & mask) == mask);

        QTableWidgetItem *item = ui->pinmapTable->item(row, PINMAP_COL_MODE);
        item->setText(isSet ? "   Output →" : "← Input     ");

        item->setBackgroundColor(QColor(isSet ? Qt::yellow : Qt::gray));
    }

    // Refresh the output cell by reading PORT
    if (forcedUpdate || _reg.changed(info.addr_port)) {
        bool isSet = ((_reg[info.addr_port] & mask) == mask);

        QTableWidgetItem *item = ui->pinmapTable->item(row, PINMAP_COL_WRITE);
        item->setText(isSet ? "High" : "Low");
        item->setBackgroundColor(QColor(isSet ? Qt::green : Qt::red));
    }

    // Refresh the ADC value for A# items
    QString name = info.name;
    if (name.startsWith("A")) {
        bool succ = false;
        int analogPin = name.remove(0, 1).toInt(&succ, 10);
        if (succ && (analogPin >= 0) && (analogPin < ANALOG_PIN_COUNT)) {
            quint16 adc_value = _reg.analog(analogPin);
            QString text = QString::number(adc_value);
            text += " (";
            text += QString::number((double) adc_value / 1023.0 * 3.3, '.', 2);
            text += "v)";

            QTableWidgetItem *adcItem = ui->pinmapTable->item(row, PINMAP_COL_ADC);
            adcItem->setText(text);
        }
    }
}

void ChipControlWidget::updateItem(QTableWidgetItem *item, bool forcedUpdate) {
    // While updating, do not cause any events
    _ignoreChanges = true;

    // Check if there are any changes or if it is forced to update
    int address = getItemAddress(item);
    bool valueError = _reg.error(address);
    bool valueChanged = _reg.changed(address);
    bool refreshNeeded = (valueChanged || valueError || forcedUpdate);

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
        if (valueError) {
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

    // We are done.
    _ignoreChanges = false;
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
    if (_ignoreChanges) return;

    int address = getItemAddress(item);
    if (item->column() == 3) {
        // The value text item was changed
        // Compare against the actual value and check for differences
        QString text = item->text();
        QString expectedText = stk500::getHexText((uint) _reg[address]);
        if (expectedText != text) {
            // Attempt to parse this value
            int value;
            bool succ = false;
            if (text.startsWith("0x", Qt::CaseInsensitive)) {
                value = text.toInt(&succ, 16);
            } else {
                value = text.toInt(&succ, 10);
            }
            if (succ) {
                _reg[address] = value;
            } else {
                item->setText(expectedText);
            }
        }

    } else {
        // Read the bitmask for the item. If there is one, stop.
        int bitMask = getItemBitMask(item);
        if (!bitMask) return;

        // Check that the item is checkable
        if (item->data(Qt::CheckStateRole).type() == QVariant::Invalid) return;

        // Toggle the bit if needed
        bool bitWasSet = (_reg[address] & bitMask) == bitMask;
        bool itemChecked = (item->checkState() == Qt::Checked);
        if (bitWasSet != itemChecked) {
            _reg[address] ^= bitMask;
        }
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

void ChipControlWidget::on_pinmapTable_cellDoubleClicked(int row, int column)
{
    const PinMapInfo &info = _reg.pinmap()[row];
    if (column == PINMAP_COL_MODE) {
        _reg[info.addr_ddr] ^= info.addr_mask;
    }
    if (column == PINMAP_COL_WRITE) {
        _reg[info.addr_port] ^= info.addr_mask;
    }
}
