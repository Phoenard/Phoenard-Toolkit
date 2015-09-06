#ifndef PORTSELECTBOX_H
#define PORTSELECTBOX_H

#include <QComboBox>
#include <QSerialPortInfo>

class PortSelectBox : public QComboBox
{
    Q_OBJECT
public:
    explicit PortSelectBox(QWidget *parent = 0);
    virtual void showPopup();
    void refreshPorts();

signals:

public slots:

};

#endif // PORTSELECTBOX_H
