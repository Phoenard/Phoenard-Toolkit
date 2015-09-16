#include "stk500port.h"

bool stk500Port::open(const QString &portName) {
    /* Initialize the port */
    port.setPortName(portName);
    bool success = port.open(QIODevice::ReadWrite);
    if (success) {
        port.setReadBufferSize(4096);
        port.setSettingsRestoredOnClose(false);
        port.setBaudRate(QSerialPort::Baud115200);
        port.setFlowControl(QSerialPort::NoFlowControl);
        port.setParity(QSerialPort::NoParity);
        port.clearError();
    }
    return success;
}

void stk500Port::close() {
    port.close();
}

void stk500Port::reset() {
    //port.close();
    //port.open(QIODevice::ReadWrite);
    port.setDataTerminalReady(true);
    port.setDataTerminalReady(false);
    port.clear();
}

void stk500Port::setBaudRate(int baud) {
    port.setBaudRate(baud);
}

bool stk500Port::isOpen() {
    return port.isOpen();
}

QString stk500Port::errorString() {
    QString text = port.errorString();
    port.clearError();
    return text;
}

QString stk500Port::portName() {
    return port.portName();
}

QByteArray stk500Port::read(int timeout) {
    const int step_timeout = 50;
    if (timeout < step_timeout) {
        return readAll(timeout);
    } else {
        qint64 start_time = QDateTime::currentMSecsSinceEpoch();
        QByteArray result;
        do {
            result = port.readAll();
            if (result.isEmpty()) {
                port.waitForReadyRead(step_timeout);
            } else {
                break;
            }
        } while ((QDateTime::currentMSecsSinceEpoch() - start_time) < timeout);

        return result;
    }
}

QByteArray stk500Port::readAll(int timeout) {
    port.waitForReadyRead(timeout);
    return port.readAll();
}

int stk500Port::write(const char* buffer, int nrOfBytes) {
    int len = port.write(buffer, nrOfBytes);
    port.flush();
    return len;
}

QList<QString> stk500Port::getPortNames() {
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    QList<QString> portNames;
    for (int i = 0; i < ports.count(); i++) {
        portNames.append(ports[i].portName());
    }
    return portNames;
}