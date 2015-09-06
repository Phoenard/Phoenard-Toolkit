#ifndef STK500_H
#define STK500_H

#include <QSerialPort>
#include <QThread>
#include <QDateTime>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QDir>
#include "stk500command.h"
#include "stk500_fat.h"
#include "stk500settings.h"

#define STK500_READ_TIMEOUT     2000   // After this much time, no response received
#define STK500_DEVICE_TIMEOUT    300   // After this much time, command mode is timed out
#define STK500_RESET_DELAY       200   // Delay between RESET and command sending
#define STK500_SERVICE_DELAY      80   // Delay between RESET and service command handling
#define STK500_CMD_MIN_INTERVAL  100   // Minimal interval of commands to stay in bootloader mode

// Pre-define components up front
class stk500sd;
class stk500registers;
class stk500service;

// Main STK500 protocol handling class
class stk500
{
public:
    stk500(QSerialPort *port = NULL);
    ~stk500();
    QSerialPort* getPort() { return port; }
    void reset();
    void resetDelayed();
    quint64 idleTime();
    bool isTimeout();
    stk500sd& sd() { return *sd_handler; }
    stk500registers& reg() { return *reg_handler; }
    stk500service& service() { return *service_handler; }

    /* Commands */
    QString signOn();
    void signOut();
    bool isSignedOn();
    bool isServiceMode();
    void setServiceMode();
    CardVolume SD_init();
    PHN_Settings readSettings();
    void writeSettings(PHN_Settings settings);
    void SD_readBlock(quint32 block, char* dest, int destLen);
    void SD_writeBlock(quint32 block, const char* src, int srcLen, bool isFAT = false);
    void FLASH_readPage(quint32 address, char* dest, int destLen);
    void FLASH_writePage(quint32 address, const char* src, int srcLen);
    void EEPROM_read(quint32 address, char* dest, int destLen);
    void EEPROM_write(quint32 address, const char* src, int srcLen);
    void RAM_read(quint16 address, char* dest, int destLen);
    void RAM_write(quint16 address, const char* src, int srcLen);
    quint8 RAM_readByte(quint16 address);
    quint8 RAM_writeByte(quint16 address, quint8 value, quint8 mask = 0xFF);
    quint16 ANALOG_read(quint8 adc);
    void SPI_transfer(const char *src, char* dest, int length);

    static QString trimFileExt(const QString &filePath);
    static QString getTempFile(const QString &filePath);
    static QString getFileName(const QString &filePath);
    static QString getFileExt(const QString &filePath);
    static QString getSizeText(quint32 size);
    static QString getTimeText(qint64 timeMillis);
    static QString getHexText(uint value);
    static void printData(char* title, char* data, int len);

private:
    /* Private commands used internally */
    int command(STK_CMD command, const char* arguments, int argumentsLength, char* response, int responseMaxLength);
    void loadAddress(quint32 address);
    void readData(STK_CMD data_command, quint32 address, char* dest, int destLen);
    void writeData(STK_CMD data_command, quint32 address, const char* src, int srcLen);

    /* Folder in %temp% where we store temporary cached files */
    static QString phnTempFolder;

    // copy ops are private to prevent copying
    stk500(const stk500&); // no implementation
    stk500& operator=(const stk500&); // no implementation

    QSerialPort *port;
    QTimer *aliveTimer;
    qint64 lastCmdTime;
    uint sequenceNumber;
    quint32 currentAddress;
    stk500sd *sd_handler;
    stk500registers *reg_handler;
    stk500service *service_handler;
    bool signedOn;
    bool serviceMode;
    QString commandNames[256];
};

// Exception thrown by the stk500 protocol if it gets into an error state
// If an exception occurs, the device must be reset and another attempt to perform the action must be taken
// If this fails a second time, the error must be reported further down the line with a meaningful message
struct ProtocolException : public std::exception {
    std::string s;
    ProtocolException() : s("") {}
    ProtocolException(QString ss) : s(ss.toStdString()) {}
    ~ProtocolException() throw () {} // Updated
    const char* what() const throw() { return s.c_str(); }
};

// Include all components after (!) stk500
#include "stk500sd.h"
#include "stk500registers.h"
#include "stk500service.h"

#endif // STK500_H
