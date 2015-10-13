#ifndef PORTSELECTBOX_H
#define PORTSELECTBOX_H

#include "phnbutton.h"
#include <QComboBox>
#include <QSerialPortInfo>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QMouseEvent>
#include <QUdpSocket>
#include <QNetworkInterface>
#include <QTimer>

typedef struct NetPortEntry {
    QString name;
    bool custom;
    int pingCtr;
} NetPortEntry;

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

protected:
    void startDiscovery();
    void endDiscovery();
    void updateNames();

protected slots:
    void runDiscovery();
    void readDiscovery();

private:
    QList<QString> portNames;
    QList<NetPortEntry> netEntries;
    bool isMouseOver;
    bool isDropDown;
    QList<QUdpSocket*> discoverSockets;
    QTimer discoverTimer;
};

#endif // PORTSELECTBOX_H
