#ifndef STK500PORT_H
#define STK500PORT_H

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QDebug>

class stk500Port
{
public:
    bool open(const QString &portName);
    void close();
    void reset();
    void setBaudRate(int baud);
    QString portName();
    QByteArray readAll(int timeout);
    QByteArray read(int timeout);
    int write(const char* buffer, int nrOfBytes);
    QString errorString();
    bool isOpen();

    static QList<QString> getPortNames();

private:
    QSerialPort port;
};

#endif // STK500PORT_H
