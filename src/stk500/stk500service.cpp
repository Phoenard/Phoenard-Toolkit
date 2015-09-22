#include "stk500service.h"

void stk500service::begin() {
    _handler->setState(STK500::SERVICE);
}

void stk500service::end() {
    _handler->setState(STK500::FIRMWARE);
}

void stk500service::checkConnection() {
    // Wait shortly and then send a simple 'HELLO' to verify echo function
    if (!writeVerifyRetry("HELLO", 5)) {
        throw ProtocolException("Lost connection with service routine");
    }
}

void stk500service::readPage(quint32 address, char* pageData) {
    setAddress(address);

    // Execute the 'r' command and read the response
    // Attempt to do this 2 times to try and read the full page
    int recLen = 0;
    char zero_data[256];
    memset(zero_data, 0, sizeof(zero_data));
    for (int i = 0; i < 2; i++) {
        if (!writeVerifyRetry("r", 1)) {
            throw ProtocolException("Failed to start reading page data");
        }
        _handler->getPort()->write(zero_data, 256);
        recLen = this->readSerial(pageData, 256);
        if (recLen == 256) {
            return;
        }
    }

    // Could not get a full response for the page
    QString errorMessage = QString("Failed to read the full 256 byte page\n"
                                   "Received: %1 bytes").arg(recLen);
    throw ProtocolException(errorMessage);
}

void stk500service::writePage(quint32 address, const char* pageData) {
    QString addressText = QString("0x") + QString::number(address, 16);

    for (int i = 0; i < 2; i++) {
        /* Refresh address */
        setAddress(address);

        /* Enter write mode */
        if (!writeVerifyRetry("w", 1)) {
            QString err = QString("Failed to start writing page at address %1").arg(addressText);
            throw ProtocolException(err);
        }

        /* Write out in three steps: erase, fill, write */
        if (writeVerify(pageData, 1) &&
            writeVerify(pageData+1, 254) &&
            writeVerify(pageData+255, 1) ) {
            return;
        } else {
            /* On errors, flush state and retry */
            flushData();
        }
    }

    QString err = QString("Failed to write page data at address %1").arg(addressText);
    throw ProtocolException(err);
}

void stk500service::setAddress(quint32 address) {
    // Make sure communication is alright
    checkConnection();

    // Compile the address change message
    char command[5];
    command[0] = 'a';
    memcpy(command+1, &address, 4);

    // Attempt to switch address two times
    for (int i = 0; i < 2; i++) {
        if (writeVerify(command, sizeof(command))) {
            return;
        } else {
            flushData();
        }
    }

    // Error
    QString addressText = QString("0x") + QString::number(address, 16);
    QString errorMessage = QString("Failed to switch to address %1").arg(addressText);
    throw ProtocolException(errorMessage);
}

void stk500service::flushData() {
    char tmp[512];
    memset(tmp, 0, sizeof(tmp));
    _handler->getPort()->write(tmp, sizeof(tmp));
    readSerial(tmp, sizeof(tmp));
}

int stk500service::readSerial(char* output, int limit) {
    int readLength = 0;
    stk500Port *port = _handler->getPort();
    while (readLength < limit) {
        QByteArray data = port->read(250);
        int remaining = limit - readLength;
        int len = (data.length() > remaining) ? remaining : data.length();
        if (!len) break;
        memcpy(output + readLength, data.data(), len);
        readLength += len;
    }
    return readLength;
}

bool stk500service::writeVerifyRetry(const char* data, int dataLen) {
    for (int i = 0; i < 2; i++) {
        if (writeVerify(data, dataLen)) {
            return true;
        } else {
            flushData();
        }
    }
    return false;
}

bool stk500service::writeVerify(const char* data, int dataLen) {
    // Write
    _handler->getPort()->write(data, dataLen);

    // Verify
    char* tmp = new char[dataLen];
    bool success = false;
    if (readSerial(tmp, dataLen) == dataLen) {
        success = (memcmp(data, tmp, dataLen) == 0);
    }
    delete[] tmp;
    return success;
}
