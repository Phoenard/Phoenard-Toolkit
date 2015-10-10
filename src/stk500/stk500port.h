#ifndef STK500PORT_H
#define STK500PORT_H

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include <QDateTime>
#include <QDebug>
#include <QThread>

// Amount of time (in ms) spent doing a single reading cycle
#define PORT_READ_STEP_TIME 5

class stk500Port
{
public:
    stk500Port();
    ~stk500Port();
    bool open(const QString &portName);
    void close();
    void clear();
    void reset();
    qint32 baudRate();
    void setBaudRate(qint32 baud);
    void waitBaudCycles(int nrOfBytes);
    QString portName();
    QByteArray readAll(int timeout);
    QByteArray read(int timeout);
    QByteArray readStep();
    int write(const char* buffer, int nrOfBytes);
    QString errorString();
    bool isOpen();
    bool isNet() const { return isNetMode; }
    bool isSerialPort() const;

    static QList<QString> getPortNames();

private:
    QIODevice *device;
    bool isNetMode;
    QString errorStr;
};

#endif // STK500PORT_H
