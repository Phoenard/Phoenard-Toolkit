#include "stk500.h"
#include <QDebug>

stk500::stk500(QSerialPort *port)
{
    this->port = port;
    this->lastCmdTime = 0;
    this->sd_handler = new stk500sd(this);
    this->signedOn = false;
}

void stk500::printData(char* title, char* data, int len) {
    QString msg = "";
    for (int i = 0; i < len; i++) {
        QString num = QString::number((uint) (uchar) data[i]);
        while (num.length() < 3) {
            num.insert(0, '0');
        }
        msg.append(num);
        msg.append(' ');
    }
    qDebug() << title << msg;
}

void stk500::resetDelayed() {
    lastCmdTime = 0;
}

void stk500::reset() {
    /* Reset state variables */
    sequenceNumber = 0;
    currentAddress = 0;
    sd_handler->reset();

    /* Reset the device and clear the buffers */
    port->setDataTerminalReady(true);
    port->setDataTerminalReady(false);

    /* Wait for the reset time */
    QThread::msleep(STK500_RESET_DELAY);

    /* Clear serial buffers */
    port->clear();

    /* Reset timeout to prevent successive resetting */
    lastCmdTime = QDateTime::currentMSecsSinceEpoch();
}

quint64 stk500::idleTime() {
    return (QDateTime::currentMSecsSinceEpoch() - lastCmdTime);
}

bool stk500::isTimeout() {
    return idleTime() > STK500_DEVICE_TIMEOUT;
}

int stk500::command(STK_CMD command, const char* arguments, int argumentsLength, char* response, int responseMaxLength) {

    // If bootloader timed out, reset the device first
    if (isTimeout()) {
        reset();
    }

    // Increment sequence number
    sequenceNumber++;
    if (sequenceNumber > 0xFF) {
        sequenceNumber = 0;
    }

    // Read length to be written out and reset position to beginning
    quint16 message_length = argumentsLength + 1;
    quint16 total_length = message_length + 6;

    // Build up a message to send out
    char* data = new char[total_length];
    data[0] = MESSAGE_START;
    data[1] = (char) (sequenceNumber & 0xFF);
    data[2] = (char) ((message_length >> 8) & 0xFF);
    data[3] = (char) (message_length & 0xFF);
    data[4] = TOKEN;
    data[5] = (char) command;

    // Fill with message data
    memcpy(data + 6, arguments, argumentsLength);

    // Calculate and append CRC
    char crc = 0x0;
    for (int i = 0; i < (total_length - 1); i++) {
        crc ^= data[i];
    }
    data[total_length - 1] = crc;

    // Send out the data, then discard it once sent
    port->write(data, total_length);
    delete[] data;

    // Read the response
    char incomingData[256];
    qint64 incomingRead;
    qint64 totalRead = 0;
    int msgParseState = ST_START;
    unsigned char checksum = 0x0;
    char status = 0;
    quint16 respLength = 0, respLength_st1 = 0;
    bool processed = false;
    QString errorInfo = "";

    // For timing measurement purposes
    qint64 commandStartTime = QDateTime::currentMSecsSinceEpoch();
    qint64 time = commandStartTime;

    while (!processed) {

        /* Read in received response data */
        incomingRead = port->read(incomingData, sizeof(incomingData));
        if (incomingRead == -1) {
            errorInfo = port->errorString();
            break;
        }

        /* Increment total received bytes counter */
        totalRead += incomingRead;

        /* Handle command read timeout */
        time = QDateTime::currentMSecsSinceEpoch();
        if ((time - commandStartTime) > STK500_READ_TIMEOUT) {
            if (errorInfo.isEmpty()) {
                errorInfo = "Read timeout";
            }
            break;
        }

        /* If nothing read, process I/O to read in new data */
        if (!incomingRead) {
            port->waitForReadyRead(50);
            continue;
        }

        /* On error, skip further processing */
        if (!errorInfo.isEmpty()) {
            continue;
        }

        // Update last command time
        lastCmdTime = time;

        /* Got data, process received bytes */
        for (int i = 0; i < incomingRead; i++) {
            unsigned char c = (unsigned char) incomingData[i];

            /* Process the incoming byte data */
            /* ============================== */

            /* Update the checksum */
            checksum ^= c;

            switch (msgParseState) {
                case ST_START:
                    if (c == MESSAGE_START) {
                        msgParseState = ST_GET_SEQ_NUM;
                    }
                    break;

                case ST_GET_SEQ_NUM:
                    if (c != sequenceNumber) {
                        errorInfo = "Sequence error mismatch";
                        break;
                    }
                    msgParseState = ST_MSG_SIZE_1;
                    break;

                case ST_MSG_SIZE_1:
                    respLength = c << 8;
                    msgParseState = ST_MSG_SIZE_2;
                    break;

                case ST_MSG_SIZE_2:
                    respLength |= c;
                    msgParseState = ST_GET_TOKEN;
                    break;

                case ST_GET_TOKEN:
                    /* Restart if token check fails */
                    if (c != TOKEN) {
                        errorInfo = "Token check failed";
                        break;
                    }
                    /* Restart if response length too short */
                    if (respLength < 2) {
                        errorInfo = "Response too short";
                        break;
                    }
                    respLength -= 2; // Exclude command/status bytes
                    msgParseState = ST_GET_CMD;
                    break;

                case ST_GET_CMD:
                    if (c != command) {
                        errorInfo = "Response command incorrect";
                        break;
                    }
                    msgParseState = ST_GET_STATUS;
                    break;

                case ST_GET_STATUS:
                    status = c;
                    if (respLength) {
                        msgParseState = ST_GET_DATA;
                    } else {
                        msgParseState = ST_GET_CHECK;
                    }
                    break;

                case ST_GET_DATA:
                    if (respLength_st1 < responseMaxLength) {
                        response[respLength_st1] = c;
                    }
                    if (++respLength_st1 == respLength) {
                        msgParseState = ST_GET_CHECK;
                    }
                    break;

                case ST_GET_CHECK:
                    /*
                     * Check if the checksum matches
                     * If checksum ^ expected == 0 it was correct
                     * Nonzero indicates the checksum was incorrect
                     * If this passes, continue processing the message data
                     * If this fails, restart this stage from the beginning
                     *
                     * In addition, check for a valid message response
                     */
                    if (checksum != 0) {
                        errorInfo = "Checksum error";
                    } else {
                        processed = true;
                    }
                    break;
            }
            /* ============================== */
        }
    }
    QString cmdCode = QString("%1").arg((uint) command, 0, 16).toUpper();
    if (cmdCode.length() == 1) {
        cmdCode.insert(0, '0');
    }
    cmdCode.insert(0, "0x");
    if (!processed) {
        // Log the error
        if (errorInfo.isEmpty()) {
            errorInfo = "None";
        }
        QString errorMessage;
        if (totalRead) {
            errorMessage = QString("Invalid response for command %1: %2\nReceived: %3 bytes, at state %4 sequence %5")
                    .arg(cmdCode).arg(errorInfo).arg(totalRead).arg(msgParseState).arg(sequenceNumber);
        } else {
            errorMessage = QString("No response for command %1").arg(cmdCode);
        }

        throw ProtocolException(errorMessage);
    } else if (status != STATUS_CMD_OK) {
        // An error occurred processing the command
        QString errorMessage = QString("Command %1 was not recognized or an error occurred processing it").arg(cmdCode);
        throw ProtocolException(errorMessage);
    } else {
        // Success! Return the received response length.
        //qDebug() << "Responsetime for command " << cmdCode << ": " << (time - commandStartTime) << "MS";
        return respLength;
    }
}

void stk500::loadAddress(quint32 address) {
    if (currentAddress == address) {
        return;
    }
    char arguments[4];
    arguments[0] = (char) ((address >> 24) & 0xFF);
    arguments[1] = (char) ((address >> 16) & 0xFF);
    arguments[2] = (char) ((address >> 8) & 0xFF);
    arguments[3] = (char) ((address >> 0) & 0xFF);
    command(LOAD_ADDRESS, arguments, sizeof(arguments), NULL, 0);
    currentAddress = address;
}

void stk500::readData(STK_CMD data_command, quint32 address, char* dest, int destLen) {
    /* Load the address (if needed) */
    loadAddress(address);

    /* Perform the read command, passing along the size to read */
    char arguments[2];
    arguments[0] = (char) ((destLen >> 8) & 0xFF);
    arguments[1] = (char) ((destLen >> 0) & 0xFF);
    command(data_command, arguments, sizeof(arguments), dest, destLen);
}

void stk500::writeData(STK_CMD data_command, quint32 address, const char* src, int srcLen) {
    /* Load the address (if needed) */
    loadAddress(address);

    /* Perform the write command, passing along the size and data to write */
    char arguments[1024];
    arguments[0] = (char) ((srcLen >> 8) & 0xFF);
    arguments[1] = (char) ((srcLen >> 0) & 0xFF);

    // Then there are 7 unused bytes, pad them with 0
    memset(arguments + 2, 0, 7);

    // Then the data
    memcpy(arguments + 9, src, srcLen);
    command(data_command, arguments, srcLen + 9, NULL, 0);
}

QString stk500::signOn() {
    char resp[100];
    int name_length = command(SIGN_ON, NULL, 0, resp, sizeof(resp)) - 1;
    if (resp[0] < name_length) {
        name_length = resp[0];
    }
    signedOn = true;
    return QString::fromLocal8Bit(resp + 1, name_length);
}

void stk500::signOut() {
    command(LEAVE_PROGMODE_ISP, NULL, 0, NULL, 0);
    signedOn = false;
}

bool stk500::isSignedOn() {
    return signedOn && !isTimeout();
}

CardVolume stk500::SD_init() {
    CardVolume volume;
    int respLen = command(INIT_SD_ISP, NULL, 0, (char*) &volume, sizeof(CardVolume));
    if (respLen != sizeof(CardVolume)) {
        /* Wrong size received */
        QString errorMessage;
        errorMessage.append("Unexpected response length for SD_Init command: received: ").append(respLen);
        errorMessage.append(" bytes, expected: ").append(sizeof(CardVolume)).append(" bytes");
        throw ProtocolException(errorMessage);
    }
    if (!volume.isInitialized) {
        throw ProtocolException("Failed to initialize the Micro-SD card volume.\nPlease check if a valid card is inserted.");
    }
    return volume;
}

PHN_Settings stk500::readSettings() {
    PHN_Settings result;
    EEPROM_read(EEPROM_SETTINGS_ADDR, (char*) &result, EEPROM_SETTINGS_SIZE);
    return result;
}

void stk500::writeSettings(PHN_Settings settings) {
    EEPROM_write(EEPROM_SETTINGS_ADDR, (char*) &settings, EEPROM_SETTINGS_SIZE);
}

void stk500::SD_readBlock(quint32 block, char* dest, int destLen) {
    readData(READ_SD_ISP, block, dest, destLen);
    currentAddress++;
}

void stk500::SD_writeBlock(quint32 block, const char* src, int srcLen, bool isFAT) {
    writeData(isFAT ? PROGRAM_SD_FAT_ISP : PROGRAM_SD_ISP, block, src, srcLen);
    currentAddress++;
}

void stk500::FLASH_readPage(quint32 address, char* dest, int destLen) {
    readData(READ_FLASH_ISP, address, dest, destLen);
    currentAddress += destLen / 2;
}

void stk500::FLASH_writePage(quint32 address, const char* src, int srcLen) {
    writeData(PROGRAM_FLASH_ISP, address, src, srcLen);
    currentAddress += srcLen / 2;
}

void stk500::EEPROM_read(quint32 address, char* dest, int destLen) {
    readData(READ_EEPROM_ISP, address, dest, destLen);
    currentAddress += destLen;
}

void stk500::EEPROM_write(quint32 address, const char* src, int srcLen) {
    writeData(PROGRAM_EEPROM_ISP, address, src, srcLen);
    currentAddress += srcLen;
}

void stk500::RAM_read(quint16 address, char* dest, int destLen) {
    readData(READ_RAM_ISP, address, dest, destLen);
    currentAddress += destLen;
}

void stk500::RAM_write(quint16 address, const char* src, int srcLen) {
    writeData(PROGRAM_RAM_ISP, address, src, srcLen);
    currentAddress += srcLen;
}

quint8 stk500::RAM_readByte(quint16 address) {
    char output = 0x00;
    char arguments[2];
    arguments[0] = (char) ((address >> 8) & 0xFF);
    arguments[1] = (char) ((address >> 0) & 0xFF);
    command(READ_RAM_BYTE_ISP, arguments, sizeof(arguments), &output, 1);
    return output;
}

quint8 stk500::RAM_writeByte(quint16 address, quint8 value, quint8 mask) {
    char output = 0x00;
    char arguments[4];
    arguments[0] = (char) ((address >> 8) & 0xFF);
    arguments[1] = (char) ((address >> 0) & 0xFF);
    arguments[2] = mask;
    arguments[3] = value;
    command(PROGRAM_RAM_BYTE_ISP, arguments, sizeof(arguments), &output, 1);
    return output;
}

quint16 stk500::ANALOG_read(quint8 adc) {
    char arguments[3];
    char output[2];

    /* Setup the ADC registers to start a measurement */
    arguments[0] = 0xC7 | (adc & 0xF); /* ADCSRA(0x7A) */
    arguments[1] = 0x00;               /* ADCSRB(0x7B) */
    arguments[2] = 0x40;               /* ADMUX (0x7C) */

    const bool USE_ADC_CMD = true;
    if (USE_ADC_CMD) {
        /* Use the analog read command */
        command(READ_ANALOG_ISP, arguments, sizeof(arguments), output, sizeof(output));
        return (output[0] << 8) | (output[1] & 0xFF);
    } else {
        /* Write to the register and read the ADCL/ADCH registers */
        RAM_write(0x7A, arguments, sizeof(arguments));
        RAM_read(0x78, output, sizeof(output));
        return (output[0] & 0xFF) | (output[1] << 8);
    }
}

/* ============================= Directory info =============================== */

QString DirectoryInfo::fileSizeTextLong() {
    QString rval;
    rval.append(QString::number(fileSize()));
    rval.append(" bytes");
    return rval;
}

QString stk500::getTimeText(qint64 timeMillis) {
    char* unit = "";
    int time_trunc = (timeMillis / 1000);
    if (time_trunc < 60) {
        unit = "second";
    } else if (time_trunc < (60 * 60)) {
        time_trunc /= 60;
        unit = "minute";
    } else {
        time_trunc /= 3600;
        unit = "hour";
    }
    QString rval;
    rval.append(QString::number(time_trunc));
    rval.append(' ');
    rval.append(unit);
    if (time_trunc != 1) {
        rval.append('s');
    }
    return rval;
}

QString stk500::phnTempFolder = "";

QString stk500::getTempFile(const QString &filePath) {
    // Generate the temporary folder if needed
    if (phnTempFolder.isEmpty()) {
        phnTempFolder = QDir::tempPath() + "/phntk_cache.tmp";

        // Make the directory
        QDir dir = QDir::root();
        dir.mkpath(phnTempFolder);
    }

    // Get the path
    return phnTempFolder + "/" + getFileName(filePath);
}

QString stk500::getFileName(const QString &filePath) {
    QString fileName = filePath;
    int endIdx;
    if (fileName.endsWith('/')) {
        endIdx = fileName.lastIndexOf('/', fileName.length() - 2);
    } else {
        endIdx = fileName.lastIndexOf('/');
    }
    if (endIdx != -1) {
        fileName.remove(0, endIdx + 1);
    }
    return fileName;
}

QString stk500::trimFileExt(const QString &filePath) {
    return filePath.left(filePath.length() - getFileExt(filePath).length());
}

QString stk500::getFileExt(const QString &filePath) {
    /* Find last dot in path */
    int dotIdx = filePath.lastIndexOf('.');
    if (dotIdx == -1) return "";

    /* Check that no file separator is put after the dot */
    if (filePath.indexOf('/', dotIdx) != -1) return "";
    if (filePath.indexOf('\\', dotIdx) != -1) return "";

    /* Valid extension */
    return filePath.right(filePath.length() - dotIdx);
}

QString stk500::getSizeText(quint32 size) {
    float size_trunc = 0.0F;
    char* unit = "";
    if (size < 1024) {
        size_trunc = size;
        unit = "bytes";
    } else if (size < (1024 * 1024)) {
        size_trunc = size / (float) 1024;
        unit = "KB";
    } else if (size < (1024 * 1024 * 1024)) {
        size_trunc = size / (float) (1024 * 1024);
        unit = "MB";
    } else {
        size_trunc = size / (float) (1024 * 1024 * 1024);
        unit = "GB";
    }
    QString rval;
    rval.append(QString::number(size_trunc, 'g', 3));
    rval.append(' ');
    rval.append(unit);
    return rval;
}
