#include "stk500.h"
#include <QDebug>

stk500::stk500()
{
    this->lastCmdTime = 0;
    this->sd_handler = new stk500sd(this);
    this->reg_handler = new stk500registers(this);
    this->service_handler = new stk500service(this);
    this->signedOn = false;
    this->lastResetTime = QDateTime::currentMSecsSinceEpoch() - STK500_MIN_RESET_TIME;
    this->currentState = STK500::UNOPENED;

    // Initialize the command names table
    QFile cmdFile(":/data/commands.csv");
    if (cmdFile.open(QIODevice::ReadOnly)) {
        QTextStream textStream(&cmdFile);
        while (!textStream.atEnd()) {
            // Read the next entry
            QStringList fields = textStream.readLine().split(",");
            if (fields.length() == 2) {
                bool succ = false;
                int command = fields[1].toInt(&succ, 16);
                if (succ && (command >= 0) && (command < 256)) {
                    commandNames[command] = fields[0];
                }
            }
        }
        cmdFile.close();
    }
}

stk500::~stk500() {
    delete this->sd_handler;
    delete this->reg_handler;
    delete this->service_handler;
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

void stk500::resetFirmware() {
    lastCmdTime = 0;
}

void stk500::open(const QString& portName) {
    if (!port.open(portName)) {
        QString err = QString("Could not open port: %1").arg(port.errorString());
        throw ProtocolException(err);
    }
}

void stk500::reset() {
    /* Reset state variables */
    sequenceNumber = 0;
    currentAddress = 0;
    sd_handler->reset();

    /* Reset baud rate */
    port.setBaudRate(STK500_BAUD);

    /* Reset the device and wait until it's ready, reading the data */
    port.reset();
    QByteArray data = port.readAll(STK500_RESET_DELAY);
    if (!data.isEmpty()) {
        QString dataText = data;
        QString errorMessage = QString("Failed to reset firmware: "
                                       "Data was received unexpectedly\n\n"
                                       "Data: %1").arg(dataText);
        throw ProtocolException(errorMessage);
    }

    /* Try to get some kind of communication going, */
    const int MAX_TRIALS = 5;
    currentState = STK500::NONE;
    for (int trial = 0; trial < MAX_TRIALS && (currentState == STK500::NONE); trial++) {

        /*
         * Write out a command which excludes the service w/r characters
         * The bytes 0x77 and 0x72 should not be used to prevent accidental writing
         *
         * Read the response back. If it is exactly the same as the data we sent, we
         * are in service mode. If it is the same as the expected response, then we
         * successfully entered firmware mode. If it is different, then something awful
         * happened and we did not reset the device into a workable state.
         */
        unsigned char test_command[11] = {0x1B, 0x00, 0x00, 0x05, 0x0E, 0x06,
                                          0x00, 0x00, 0x00, 0x00, 0x16};
        unsigned char test_response[8] = {0x1B, 0x00, 0x00, 0x02,
                                           0x0E, 0x06, 0x00, 0x11};

        port.clear();
        port.write((char*) test_command, sizeof(test_command));
        port.waitBaudCycles((int) sizeof(test_command));

        lastCmdTime = QDateTime::currentMSecsSinceEpoch();
        QByteArray response;
        response.reserve(11);
        do {
            response.append(port.read(50));

            // Check momentarily what kind of response is received
            if (memcmp(response.data(), test_response, sizeof(test_response)) == 0) {
                currentState = STK500::FIRMWARE;
                break;
            }
            if (memcmp(response.data(), test_command, sizeof(test_command)) == 0) {
                currentState = STK500::SERVICE;
                break;
            }
        } while ((QDateTime::currentMSecsSinceEpoch() - lastCmdTime) < 300);

        /* Reset timeout to prevent successive resetting */
        lastResetTime = lastCmdTime = QDateTime::currentMSecsSinceEpoch();
    }
    if (currentState == STK500::NONE) {
        QString errorMessage = QString("Failed to reset firmware: "
                                       "Unable to establish connection");
        throw ProtocolException(errorMessage);
    }

    /* Test alternative baud rate operation here */
    //setBaudRate(300);
}

void stk500::setBaudRate(qint32 baud) {
    if (baud == port.baudRate()) {
        return;
    }

    // Set UART0 to the specified baud rate
    ChipRegisters reg;
    reg.setupUART(0, baud);

    // Find the block of memory where changes were made
    int address = 0;
    int dataLength = 0;
    reg.getChangedRange(&address, &dataLength);

    /* Logging for debugging */
    const bool log_ram_changes = false;
    if (log_ram_changes) {
        qDebug() << "WRITING OUT " << address << " LEN " << dataLength;
        for (int i = 0; i < dataLength; i++) {
            qDebug() << "  [" << getHexText(address+i) << "] = " << getHexText(reg.data(address)[i]);
        }
    }

    /* Load the address (if needed) */
    loadAddress(address);

    /* Perform the write command, passing along the size and data to write */
    char arguments[1024];
    arguments[0] = (char) (((quint16) dataLength >> 8) & 0xFF);
    arguments[1] = (char) (((quint16) dataLength >> 0) & 0xFF);

    // Then there are 7 unused bytes, pad them with 0
    memset(arguments + 2, 0, 7);

    // Then the data
    memcpy(arguments + 9, reg.data(address), dataLength);

    // Write out the command, and switch baud rate.
    // It is impossible to read the response; it is received
    // at the wrong baud rate. Instead of receiving, wait until
    // the response data is received. This is done by waiting for
    // the calculated time-to-send.
    commandWrite(STK500::PROGRAM_RAM_ISP, arguments, dataLength + 9);
    port.setBaudRate(baud);
    port.waitBaudCycles(10);
    port.clear();
    currentAddress += dataLength;
    lastCmdTime = QDateTime::currentMSecsSinceEpoch();

    // Verify that the connection is preserved
    signOn();
}

void stk500::setState(STK500::State newState, qint32 baudRate) {
    /* Check if there are any changes at all */
    if ((newState == currentState) && (baudRate == port.baudRate())) {
        return;
    }

    if (newState == STK500::UNOPENED) {
        // Close the port
        port.close();

    } else if (newState == STK500::NONE) {
        // Simply change the settings but do nothing else
        port.setBaudRate(baudRate);

    } else if (newState == STK500::SERVICE) {
        // Write out the SERVICE_MODE command
        commandWrite(STK500::SERVICE_MODE_ISP, NULL, 0);

        // Verify shortly that no response is returned
        // If there is one, it means the command was normally executed
        if (!port.readAll(250).isEmpty()) {
            throw ProtocolException("Service mode is not supported by the firmware on the device."
                                    "The SERVICE_MODE command gave an unexpected response.\n\n"
                                    "Perhaps outdated firmware is installed?");
        }

        // Verify that a connection is established by echo
        port.write("HELLO", 5);
        QString response = port.readAll(50);
        if (response.isEmpty()) {
            throw ProtocolException("Failed to establish a connection with the service routine."
                                    "No response was returned from the device.\n\n"
                                    "Perhaps outdated firmware is installed?");

        } else if (response != "HELLO") {
            QString errorMessage = QString("Failed to establish a connection with the service routine."
                                           "An invalid response was returned from the device.\n\n"
                                           "%1\n\n"
                                           "Perhaps outdated firmware is installed?").arg(response);
            throw ProtocolException(errorMessage);
        }

    } else if (newState == STK500::SKETCH) {
        // If not currently in firmware mode, first reset
        if (currentState != STK500::FIRMWARE) {
            reset();
        }

        // Sign out to instantly go to the sketch
        signOut();

        // Switch baud rate
        port.setBaudRate(baudRate);

    } else {
        // Simply sign on to verify current connection / put into firmware mode
        signOn();

        // Switch to multi-serial mode for some states
        // Also turn devices on/off as required
        int multiSerialIdx = -1;
        if (newState == STK500::SIM_GSM) {
            multiSerialIdx = 1;

        } else if (newState == STK500::SIM_GPS) {
            multiSerialIdx = 3;

        } else if (newState == STK500::WIFI) {
            multiSerialIdx = 2;
            // Turn off Bluetooth/turn on WiFi
            ChipRegisters reg;
            reg.setPin("WiFi", "Power Key", true, true);
            reg.setPin("BlueTooth", "Reset", true, false);
            this->reg().write(reg);

        } else if (newState == STK500::BLUETOOTH) {
            multiSerialIdx = 2;
            // Turn off WiFi/turn on Bluetooth
            ChipRegisters reg;
            reg.setPin("WiFi", "Power Key", true, false);
            reg.setPin("BlueTooth", "Reset", true, true);
            this->reg().write(reg);
        }

        // Special SIM powering routine (performed at full baud speed)
        if ((newState == STK500::SIM_GSM) || (newState == STK500::SIM_GPS)) {
            setBaudRate(STK500_BAUD);
            ChipRegisters reg;
            reg.setPin("Sim908", "DTRS", false, true);
            this->reg().write(reg);

            this->reg().read(reg);
            if (!reg.getPin("Sim908", "Status") && !reg.getPin("Sim908", "DTRS")) {
                reg.setPin("Sim908", "Power Key", true, true);
                this->reg().write(reg);

                qint64 timeOut = QDateTime::currentMSecsSinceEpoch();
                do {
                    signOn();
                    QThread::msleep(STK500_CMD_MIN_INTERVAL);
                } while ((QDateTime::currentMSecsSinceEpoch() - timeOut) < 1500);

                reg.setPin("Sim908", "Power Key", true, false);
                this->reg().write(reg);
            }
        }

        // Switch firmware baud rate if non-standard
        this->setBaudRate(baudRate);

        // Switch to multi-serial mode
        if (multiSerialIdx != -1) {
            SERIAL_begin(0, multiSerialIdx);
        }
    }

    // Done!
    currentState = newState;
}

quint64 stk500::firmwareIdleTime() {
    return (QDateTime::currentMSecsSinceEpoch() - lastCmdTime);
}

STK500::State stk500::state() {
    return currentState;
}

QString stk500::stateName() {
    return stateName(currentState);
}

QString stk500::stateName(STK500::State state) {
    switch (state) {
    case STK500::UNOPENED: return "-";
    case STK500::NONE: return "No Connection";
    case STK500::SKETCH: return "Sketch";
    case STK500::FIRMWARE: return "STK500";
    case STK500::SERVICE: return "Service";
    case STK500::SIM_GSM: return "Sim908 GSM";
    case STK500::SIM_GPS: return "Sim908 GPS";
    case STK500::BLUETOOTH: return "Bluetooth";
    case STK500::WIFI: return "WiFi";
    default:
        return "Unknown";
    }
}

bool stk500::isFirmwareTimeout() {
    if ((QDateTime::currentMSecsSinceEpoch() - lastResetTime) < STK500_MIN_RESET_TIME) {
        return false;
    }
    if (currentState != STK500::FIRMWARE) {
        return true;
    }
    return firmwareIdleTime() > STK500_DEVICE_TIMEOUT;
}

int stk500::command(STK500::CMD command, const char* arguments, int argumentsLength, char* response, int responseMaxLength) {

    // Write out the command (also arranges firmware initialization)
    commandWrite(command, arguments, argumentsLength);

    // Read the response
    qint64 totalRead = 0;
    qint64 cmdStartTime = QDateTime::currentMSecsSinceEpoch();
    quint16 respLength = 0;
    bool processed = false;
    bool hasResponse = false;
    QString errorInfo = "No Response";
    QByteArray receivedData;
    do {

        /* Read in received response data */
        QByteArray newData = port.read(STK500_READ_TIMEOUT);
        receivedData.append(newData);

        /* If no data is received at this point, we will not expect any */
        if (newData.isEmpty()) {
            break;
        }

        /* Handle command read timeout if no response is received */
        lastCmdTime = QDateTime::currentMSecsSinceEpoch();
        if (!hasResponse && ((lastCmdTime-cmdStartTime) > STK500_READ_TIMEOUT)) {
            break;
        }

        /* Process full response */
        errorInfo = readCommandResponse(command, receivedData, response, responseMaxLength, &hasResponse);
        if (errorInfo.startsWith("RESP=")) {
            respLength = errorInfo.remove(0, 5).toInt();
            processed = true;
        }

        /* Abort if too much data is received (ERR_OVERFLOW) */
        if (receivedData.length() > 800) {
            break;
        }
    } while (!processed);

    totalRead = receivedData.length();

    // Handle (the lack of) the response
    QString cmdName = commandNames[command] + " (" + getHexText((uint) command) + ")";
    if (!processed) {
        // Log the error
        QString errorMessage;
        errorMessage = QString("Failed to execute command %1: %2\nReceived: %3 bytes, sequence %4")
                .arg(cmdName).arg(errorInfo).arg(totalRead).arg(sequenceNumber);

        const bool report_data = false;
        if (report_data) {
            errorMessage += "\n\n";
            for (int i = 0; i < receivedData.length(); i++) {
                errorMessage += "[";
                errorMessage += QString::number((uint) (unsigned char) receivedData[i], 10);
                errorMessage += "] ";
            }
        }

        const bool log_errors = true;
        if (log_errors) {
            QStringList errorLines = errorMessage.split("\n");
            for (int i = 0; i < errorLines.count(); i++) {
                qDebug() << errorLines[i].toStdString().c_str();
            }
        }

        throw ProtocolException(errorMessage);

    } else {
        // Success: increment sequence number
        sequenceNumber++;
        if (sequenceNumber > 0xFF) {
            sequenceNumber = 0;
        }

        // Success! Return the received response length.
        //qDebug() << "Command finished: " << cmdName;
        return respLength;
    }
}

void stk500::commandWrite(STK500::CMD command, const char* arguments, int argumentsLength) {
    // If bootloader timed out, reset the device first
    if (isFirmwareTimeout()) {
        reset();
    }

    // Fail if we are not in firmware mode at all
    if (currentState != STK500::FIRMWARE) {
        QString err = QString("Can not send command because device is in %1 mode").arg(stateName());
        throw ProtocolException(err);
    }

    // Read length to be written out and reset position to beginning
    quint16 message_length = argumentsLength + 1;
    quint16 total_length = message_length + 6;

    // Build up a message to send out
    char* data = new char[total_length];
    data[0] = STK500::MESSAGE_START;
    data[1] = (char) (sequenceNumber & 0xFF);
    data[2] = (char) ((message_length >> 8) & 0xFF);
    data[3] = (char) (message_length & 0xFF);
    data[4] = STK500::TOKEN;
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
    port.write(data, total_length);
    delete[] data;

    // This short delay is needed to guarantee data is written before receiving
    // Especially important in low-baud rate operation mode to include TX delay
    port.waitBaudCycles(total_length);
}

QString stk500::readCommandResponse(STK500::CMD command, const QByteArray &input,
                                    char* response, int responseMaxLength, bool* hasResponse) {

    *hasResponse = false;

    const int MIN_MSG_LEN = 8;
    if (input.length() == 0) {
        *hasResponse = false;
        return "No response received";
    }
    if (input.length() < MIN_MSG_LEN) {
        *hasResponse = false;
        QString lenTxt = QString::number(input.length());
        return QString("Response too short (%1 bytes)").arg(lenTxt);
    }

    int level = 0;
    quint16 respLength;
    QString errorMessage = "No message start token found";
    for (int offset = 0; offset <= (input.length() - MIN_MSG_LEN); offset++) {
        const unsigned char* data = (const unsigned char*) (input.data() + offset);
        int dataLength = input.length() - offset;
        // =======================================================

        // Verify the start token is present
        if (data[0] != STK500::MESSAGE_START) {
            continue;
        }

        // Verify sequence number matches up
        if (data[1] != this->sequenceNumber) {
            if (level < 1) {
                level = 1;
                errorMessage = "Sequence number mismatch (";
                errorMessage += QString::number(data[1], 10);
                errorMessage += " != ";
                errorMessage += QString::number(this->sequenceNumber, 10);
                errorMessage += ")";
            }
            continue;
        }

        // Token check
        if (data[4] != STK500::TOKEN) {
            if (level < 2) {
                level = 2;
                errorMessage = "Message token invalid (";
                errorMessage += QString::number(data[4], 10);
                errorMessage += " != ";
                errorMessage += QString::number(STK500::TOKEN, 10);
                errorMessage += ")";
            }
            continue;
        }

        // Command ID check
        if (data[5] != command) {
            if (level < 3) {
                level = 3;
                errorMessage = "Message command ID invalid (";
                errorMessage += QString::number(data[5], 10);
                errorMessage += " != ";
                errorMessage += QString::number(command, 10);
                errorMessage += ")";
            }
            continue;
        }

        // Status OK Check
        if (data[6] != STK500::STATUS_CMD_OK) {
            if (level < 4) {
                level = 4;
                errorMessage = "An error occurred while processing the command on the device";
            }
            continue;
        }

        // Read and verify the message size parameter
        respLength = data[2] << 8;
        respLength |= data[3];
        if (respLength < 2) {
            if (level < 5) {
                level = 5;
                QString lenText = QString::number(respLength, 10);
                errorMessage = QString("Message header length too short (len: %1)").arg(lenText);
            }
            continue;
        }
        if (respLength > (dataLength-6)) {
            if (level < 5) {
                level = 5;

                // Data can still be expected at this point
                *hasResponse = true;
                QString lenText = QString::number(respLength, 10);
                errorMessage = QString("Response too short: message header indicates "
                                       "length longer than the response (len: %1)").arg(lenText);
            }
            continue;
        }

        // Verify CRC
        unsigned char crc = 0x0;
        quint16 msg_end_idx = (MIN_MSG_LEN - 3 + respLength);
        for (quint16 i = 0; i < msg_end_idx; i++) {
            crc ^= data[i];
        }
        if (data[msg_end_idx] != crc) {
            if (level < 6) {
                level = 6;
                // Data corrupted, no more response
                *hasResponse = false;
                errorMessage = "Message CRC check failed (";
                errorMessage += QString::number(data[msg_end_idx], 10);
                errorMessage += " != ";
                errorMessage += QString::number(crc, 10);
                errorMessage += ")";
            }
            continue;
        }

        // All alright; read in the data and reply with RESP: [len]
        memcpy(response, data + 7, std::min(responseMaxLength, (int) respLength));
        errorMessage = QString("RESP=%1").arg(respLength-2);
        break;

    }
    return errorMessage;
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
    command(STK500::LOAD_ADDRESS, arguments, sizeof(arguments), NULL, 0);
    currentAddress = address;
}

void stk500::readData(STK500::CMD data_command, quint32 address, char* dest, int destLen) {
    /* Load the address (if needed) */
    loadAddress(address);

    /* Perform the read command, passing along the size to read */
    char arguments[2];
    arguments[0] = (char) ((destLen >> 8) & 0xFF);
    arguments[1] = (char) ((destLen >> 0) & 0xFF);
    command(data_command, arguments, sizeof(arguments), dest, destLen);
}

void stk500::writeData(STK500::CMD data_command, quint32 address, const char* src, int srcLen) {
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
    int name_length = command(STK500::SIGN_ON, NULL, 0, resp, sizeof(resp)) - 1;
    if (resp[0] < name_length) {
        name_length = resp[0];
    }
    signedOn = true;
    return QString::fromLocal8Bit(resp + 1, name_length);
}

void stk500::signOut() {
    command(STK500::LEAVE_PROGMODE_ISP, NULL, 0, NULL, 0);
    signedOn = false;
}

bool stk500::isSignedOn() {
    return (currentState == STK500::FIRMWARE) && signedOn && !isFirmwareTimeout();
}

CardVolume stk500::SD_init() {
    CardVolume volume;
    int respLen = command(STK500::INIT_SD_ISP, NULL, 0, (char*) &volume, sizeof(CardVolume));
    if (respLen != sizeof(CardVolume)) {
        /* Wrong size received */
        QString errorMessage;
        QString sz_rec = QString::number(respLen, 10);
        QString sz_exp = QString::number((int) sizeof(CardVolume), 10);
        errorMessage.append("Unexpected response length for SD_Init command: received: ").append(sz_rec);
        errorMessage.append(" bytes, expected: ").append(sz_exp).append(" bytes");
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
    readData(STK500::READ_SD_ISP, block, dest, destLen);
    currentAddress++;
}

void stk500::SD_writeBlock(quint32 block, const char* src, int srcLen, bool isFAT) {
    writeData(isFAT ? STK500::PROGRAM_SD_FAT_ISP : STK500::PROGRAM_SD_ISP, block, src, srcLen);
    currentAddress++;
}

void stk500::FLASH_readPage(quint32 address, char* dest, int destLen) {
    readData(STK500::READ_FLASH_ISP, address >> 1, dest, destLen);
    currentAddress += destLen / 2;
}

void stk500::FLASH_writePage(quint32 address, const char* src, int srcLen) {
    writeData(STK500::PROGRAM_FLASH_ISP, address >> 1, src, srcLen);
    currentAddress += srcLen / 2;
}

void stk500::EEPROM_read(quint32 address, char* dest, int destLen) {
    readData(STK500::READ_EEPROM_ISP, address, dest, destLen);
    currentAddress += destLen;
}

void stk500::EEPROM_write(quint32 address, const char* src, int srcLen) {
    writeData(STK500::PROGRAM_EEPROM_ISP, address, src, srcLen);
    currentAddress += srcLen;
}

void stk500::RAM_read(quint16 address, char* dest, int destLen) {
    while (destLen) {
        int step = std::min(512, destLen);
        readData(STK500::READ_RAM_ISP, address, dest, step);
        currentAddress += step;
        address += step;
        dest += step;
        destLen -= step;
    }
}

void stk500::RAM_write(quint16 address, const char* src, int srcLen) {
    writeData(STK500::PROGRAM_RAM_ISP, address, src, srcLen);
    currentAddress += srcLen;
}

quint8 stk500::RAM_readByte(quint16 address) {
    char output = 0x00;
    char arguments[2];
    arguments[0] = (char) ((address >> 8) & 0xFF);
    arguments[1] = (char) ((address >> 0) & 0xFF);
    command(STK500::READ_RAM_BYTE_ISP, arguments, sizeof(arguments), &output, 1);
    return output;
}

quint8 stk500::RAM_writeByte(quint16 address, quint8 value, quint8 mask) {
    char output = 0x00;
    char arguments[4];
    arguments[0] = (char) ((address >> 8) & 0xFF);
    arguments[1] = (char) ((address >> 0) & 0xFF);
    arguments[2] = mask;
    arguments[3] = value;
    command(STK500::PROGRAM_RAM_BYTE_ISP, arguments, sizeof(arguments), &output, 1);
    return output;
}

quint16 stk500::ANALOG_read(quint8 pin) {
    char output[2];
    int analog_reference = 1;

    /* allow for channel or pin numbers */
    if (pin >= 54) pin -= 54;

    /* Setup the ADC registers to start a measurement */
    unsigned char adcsra = 0x87 | (1 << 6);
    unsigned char adcsrb = (pin & 0x8);
    unsigned char admux = (analog_reference << 6) | (pin & 0x07);

    /* Use the analog read command */
    unsigned char arguments[3] = { adcsra, adcsrb, admux };
    command(STK500::READ_ANALOG_ISP, (char*) arguments, sizeof(arguments), output, sizeof(output));
    return (output[0] << 8) | (output[1] & 0xFF);
}

void stk500::SPI_transfer(const char *src, char* dest, int length) {
    command(STK500::TRANSFER_SPI_ISP, src-1, length+1, dest, dest ? length : 0);
}

void stk500::SERIAL_begin(int serialIdxA, int serialIdxB) {
    ChipRegisters reg;
    int serial[2] = {serialIdxA, serialIdxB};
    char arguments[4];
    for (int i = 0; i < 2; i++) {
        // Acquire address to first UART register
        QString serialN = QString::number(serial[i]);
        QString regName = QString("UCSR%1A").arg(serialN);
        int registerAddr = reg.findRegisterAddress(regName);
        arguments[2*i+0] = (char) (registerAddr & 0xFF);
        arguments[2*i+1] = (char) (registerAddr >> 8);

        // Initialize baud rate if not self
        if (serial[i] != 0) {
            reg.setupUART(serial[i], port.baudRate());
        }
    }

    // Write out the register changes
    this->reg().write(reg);

    // Set device to multi-serial mode (this command has no response)
    commandWrite(STK500::MULTISERIAL_ISP, arguments, sizeof(arguments));
}

/* ============================= Directory info =============================== */

QString DirectoryInfo::fileSizeTextLong() {
    QString rval;
    rval.append(QString::number(fileSize()));
    rval.append(" bytes");
    return rval;
}

QString stk500::getTimeText(qint64 timeSeconds) {
    const char* unit = "";
    int time_trunc = 0;
    if (timeSeconds < 60) {
        time_trunc = timeSeconds;
        unit = "second";
    } else if (timeSeconds < (60 * 60)) {
        time_trunc = timeSeconds / 60;
        unit = "minute";
    } else {
        time_trunc = timeSeconds / 3600;
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
    const char* unit = "";
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

QString stk500::getHexText(uint value) {
    QString hex = QString("%1").arg((uint) value, 0, 16).toUpper();
    if (hex.length() == 1) {
        hex.insert(0, '0');
    }
    hex.insert(0, "0x");
    return hex;
}
