#ifndef CHIPCONTROLWIDGET_H
#define CHIPCONTROLWIDGET_H

#include <QWidget>
#include "mainmenutab.h"

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

private:
    void startUpdating();
    void updateEntry(int index, bool forceUpdate);

    Ui::ChipControlWidget *ui;
    stk500UpdateRegisters *lastTask;
    bool _active;
    ChipRegisters _reg;
};

#endif // CHIPCONTROLWIDGET_H
