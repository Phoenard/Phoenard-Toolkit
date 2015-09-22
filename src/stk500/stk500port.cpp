#include "stk500port.h"

bool stk500Port::open(const QString &portName) {
    /* Initialize the port */
    port.setPortName(portName);
    bool success = port.open(QIODevice::ReadWrite);
    if (success) {
        port.setReadBufferSize(4096);
        port.setSettingsRestoredOnClose(false);
        port.setFlowControl(QSerialPort::NoFlowControl);
        port.setParity(QSerialPort::NoParity);
        port.clearError();
    }
    return success;
}

void stk500Port::clear() {
    port.clear();
}

void stk500Port::close() {
    port.close();
}

void stk500Port::reset() {
    port.setDataTerminalReady(true);
    port.setDataTerminalReady(false);
    clear();
}

qint32 stk500Port::baudRate() {
    return port.baudRate();
}

void stk500Port::setBaudRate(qint32 baud) {
    port.setBaudRate(baud);
}

void stk500Port::waitBaudCycles(int nrOfBytes) {
    qint32 total_bits = nrOfBytes * 10;
    qint32 bits_per_second = baudRate();
    qint32 time_to_send_ms = 1000 * total_bits / bits_per_second;
    if (time_to_send_ms) {
        QThread::msleep(time_to_send_ms + 2);
    }
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
    qint64 start_time = QDateTime::currentMSecsSinceEpoch();
    QByteArray result;
    do {
        result = readStep();
        if (!result.isEmpty()) {
            break;
        }
    } while ((QDateTime::currentMSecsSinceEpoch() - start_time) < timeout);
    return result;
}

QByteArray stk500Port::readAll(int timeout) {
    qint64 start_time = QDateTime::currentMSecsSinceEpoch();
    QByteArray data;
    do {
        data.append(readStep());
    } while ((QDateTime::currentMSecsSinceEpoch() - start_time) < timeout);
    return data;
}

QByteArray stk500Port::readStep() {
    port.waitForReadyRead(PORT_READ_STEP_TIME);
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
