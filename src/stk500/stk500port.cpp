#include "stk500port.h"

stk500Port::stk500Port() {
    isNetMode = false;
    device = NULL;
    errorStr = "";
}

stk500Port::~stk500Port() {
    close();
}

bool stk500Port::open(const QString &portName) {
    /* Don't open anything if already open */
    if (isOpen()) {
        errorStr = "Already open";
        return false;
    }

    if (portName.startsWith("net:")) {
        /* Initialize a new socket over UDP */
        const bool useUDP = false;
        QHostAddress deviceAddr(portName.mid(4));
        QAbstractSocket *socket;
        if (useUDP) {
            socket = new QUdpSocket();
        } else {
            socket = new QTcpSocket();
        }
        socket->setSocketOption(QAbstractSocket::KeepAliveOption, true );
        socket->connectToHost(deviceAddr, 7265);
        if (socket->waitForConnected(5000)) {
            /* Swap out devices */
            delete device;
            device = socket;
            isNetMode = true;
        } else {
            errorStr = socket->errorString();
            delete socket;
            return false;
        }
    } else {
        /* Check if the port can be opened */
        QSerialPortInfo info(portName);
        if (info.isBusy()) {
            errorStr = "Port Busy";
            return false;
        }
        if (!info.isValid()) {
            errorStr = "Port Invalid";
            return false;
        }

        /* Initialize a new serial port */
        QSerialPort *port = new QSerialPort(info);
        if (port->open(QIODevice::ReadWrite)) {
            port->setReadBufferSize(4096);
            port->setSettingsRestoredOnClose(false);
            port->setFlowControl(QSerialPort::NoFlowControl);
            port->setParity(QSerialPort::NoParity);
            port->clearError();

            /* Swap out devices */
            delete device;
            device = port;
            isNetMode = false;
        } else {
            errorStr = port->errorString();
            delete port;
            return false;
        }
    }
    return true;
}

bool stk500Port::isSerialPort() const {
    return !isNetMode && (device != NULL);
}

void stk500Port::clear() {
    if (isSerialPort()) {
        ((QSerialPort*) device)->clear();
    }
}

void stk500Port::close() {
    if ((device != NULL) && device->isOpen()) {
        if (isNet()) {
            QAbstractSocket *socket = (QAbstractSocket*) device;
            socket->abort();
            socket->close();
        } else if (isSerialPort()) {
            device->close();
        }
        delete device;
        device = NULL;
    }
}

void stk500Port::reset() {
    if (isSerialPort()) {
        QSerialPort* port = (QSerialPort*) device;
        port->setDataTerminalReady(true);
        port->setDataTerminalReady(false);
    }
    clear();
}

qint32 stk500Port::baudRate() {
    if (isSerialPort()) {
        return ((QSerialPort*) device)->baudRate();
    } else {
        return 115200;
    }
}

void stk500Port::setBaudRate(qint32 baud) {
    if (isSerialPort()) {
        ((QSerialPort*) device)->setBaudRate(baud);
    }
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
    return (device != NULL) && device->isOpen();
}

QString stk500Port::errorString() {
    if (!errorStr.isEmpty()) {
        QString err = errorStr;
        errorStr = "";
        return err;
    }
    QString text = device->errorString();
    if (isSerialPort()) {
        ((QSerialPort*) device)->clearError();
    }
    return text;
}

QString stk500Port::portName() {
    if (isSerialPort()) {
        return ((QSerialPort*) device)->portName();
    } else {
        return "Unknown";
    }
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
    device->waitForReadyRead(PORT_READ_STEP_TIME);
    return device->readAll();
}

int stk500Port::write(const char* buffer, int nrOfBytes) {
    int len = device->write(buffer, nrOfBytes);
    if (isSerialPort()) {
        ((QSerialPort*) device)->flush();
    }
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
