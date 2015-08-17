#ifndef CHIPCONTROLWIDGET_H
#define CHIPCONTROLWIDGET_H

#include <QWidget>
#include "mainmenutab.h"
#include <QTableWidget>

namespace Ui {
class ChipControlWidget;
}

class ChipControlWidget : public QWidget, public MainMenuTab
{
    Q_OBJECT

public:
    explicit ChipControlWidget(QWidget *parent = 0);
    ~ChipControlWidget();
    void setActive(bool active);

private slots:
    void serialTaskFinished(stk500Task *task);

    void on_registerTable_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);

    void on_registerTable_itemChanged(QTableWidgetItem *item);

    void on_registerTable_itemDoubleClicked(QTableWidgetItem *item);

private:
    /// Triggers the automatic update loop
    void startUpdating();
    /// Refreshes the state of a single item
    void updateItem(QTableWidgetItem *item, bool forcedUpdate);
    /// Gets the bit mask for an item; returns 0 when not a bit column item
    int getItemBitMask(QTableWidgetItem *item);
    /// Gets the address associated with the row of an item
    int getItemAddress(QTableWidgetItem *item);

    Ui::ChipControlWidget *ui;
    stk500UpdateRegisters *lastTask;
    bool _active;
    ChipRegisters _reg;
};

#endif // CHIPCONTROLWIDGET_H
