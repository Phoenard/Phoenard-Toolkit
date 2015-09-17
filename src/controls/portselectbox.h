#ifndef PORTSELECTBOX_H
#define PORTSELECTBOX_H

#include "phnbutton.h"
#include <QComboBox>
#include <QSerialPortInfo>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QMouseEvent>

class PortSelectBox : public QComboBox
{
    Q_OBJECT
public:
    explicit PortSelectBox(QWidget *parent = 0);
    void refreshPorts();
    QString portName();

    virtual void showPopup();
    virtual void hidePopup();
    virtual void paintEvent(QPaintEvent *e);
    virtual void enterEvent(QEvent *event);
    virtual void leaveEvent(QEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);

private:
    bool isMouseOver;
    bool isDropDown;
};

#endif // PORTSELECTBOX_H
