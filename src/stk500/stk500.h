#ifndef STK500_H
#define STK500_H

#include <QSerialPort>
#include <QThread>
#include <QDateTime>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include "stk500command.h"
#include "stk500_fat.h"

#define STK500_READ_TIMEOUT     2000   // After this much time, no response received
#define STK500_DEVICE_TIMEOUT    300   // After this much time, command mode is timed out
#define STK500_RESET_DELAY       200   // Delay between RESET and command sending
#define STK500_CMD_MIN_INTERVAL  200   // Minimal interval of commands to stay in bootloader mode

// Forward declare some classes
class stk500_sd;
class DirectoryWalker;
class DirectoryInfo;
struct ProtocolException;

// Main STK500 protocol handling class
class stk500
{
public:
    stk500(QSerialPort *port = NULL);
    void reset();
    void resetDelayed();
    quint64 idleTime();
    bool isTimeout();
    stk500_sd& sd() { return *sd_handler; }

    /* Commands */
    QString signOn();
    void signOut();
    CardVolume SD_init();
    void SD_readBlock(quint32 block, char* dest, int destLen);
    void SD_writeBlock(quint32 block, char* src, int srcLen, bool isFAT = false);
    void FLASH_readPage(quint32 address, char* dest, int destLen);
    void FLASH_writePage(quint32 address, char* src, int srcLen);
    void EEPROM_read(quint32 address, char* dest, int destLen);
    void EEPROM_write(quint32 address, char* src, int srcLen);
    void RAM_read(quint16 address, char* dest, int destLen);
    void RAM_write(quint16 address, char* src, int srcLen);
    quint8 RAM_readByte(quint16 address);
    quint8 RAM_writeByte(quint16 address, quint8 value, quint8 mask = 0xFF);
    quint16 ANALOG_read(quint8 adc);

    static QString getFileName(QString filePath);
    static QString getFileExt(QString filePath);
    static QString getSizeText(quint32 size);
    static QString getTimeText(qint64 timeMillis);
    static void printData(char* title, char* data, int len);

private:
    /* Private commands used internally */
    int command(STK_CMD command, const char* arguments, int argumentsLength, char* response, int responseMaxLength);
    void loadAddress(quint32 address);
    void readData(STK_CMD data_command, quint32 address, char* dest, int destLen);
    void writeData(STK_CMD data_command, quint32 address, char* src, int srcLen);

    QSerialPort *port;
    QTimer *aliveTimer;
    qint64 lastCmdTime;
    uint sequenceNumber;
    quint32 currentAddress;
    stk500_sd *sd_handler;
};

// Stores a 512-byte cache for a single block of data
typedef struct BlockCache {
    char buffer[512];
    quint32 block;
    bool isFAT;
    bool needsWriting;
    int usageCtr;

    void reset() {
        block = 0xFFFFFFFF;
        needsWriting = false;
        // Set usage to negative to guarantee it being used
        usageCtr = -1;
    }
} BlockCache;

// How many caches to allocate for buffering
#define SD_CACHE_CNT 10

// Extension for dealing with Micro-SD access through STK500 protocol
class stk500_sd
{
private:
    stk500 *_handler;
    CardVolume _volume;
    BlockCache _cache[SD_CACHE_CNT];

    void writeOutCache(BlockCache *cache);
public:
    stk500_sd(stk500 *handler);
    void init(bool forceInit = false);
    void reset();
    void debugPrint();
    CardVolume volume();

    /* Cache handling */
    char* cacheBlock(quint32 block, bool readBlock, bool markDirty, bool isFat = false);
    void flushCache();
    void discardCache();
    void wipeBlock(quint32 block, bool isFat = false);
    void wipeCluster(quint32 cluster);
    void read(quint32 block, int blockOffset, char* dest, int length);
    void write(quint32 block, int blockOffset, char* src, int length);
    DirectoryEntryPtr nextDirectory(DirectoryEntryPtr dir_ptr, int count = 1, bool create = false);
    DirectoryEntry readDirectory(DirectoryEntryPtr entryPtr);
    void writeDirectory(DirectoryEntryPtr entryPtr, DirectoryEntry entry);
    void wipeDirectory(DirectoryEntryPtr entryPtr);

    /* FAT cluster handling */
    bool isEOC(quint32 cluster);
    quint32 getClusterBlock(quint32 cluster);
    quint32 getClusterFromBlock(quint32 block);
    quint32 fatGet(quint32 cluster);
    void fatPut(quint32 cluster, quint32 clusterNext);
    quint32 findFreeCluster(quint32 startCluster = 2);
    void wipeClusterChain(quint32 startCluster);

    /* File API functions */
    DirectoryEntryPtr getRootPtr();
    DirectoryEntryPtr getDirPtrFromCluster(quint32 cluster);
};

// Stores all information needed to access a file or directory
class DirectoryInfo {
private:
    DirectoryEntryPtr _firstPtr;
    DirectoryEntryPtr _entryPtr;
    DirectoryEntry _entry;
    QString _fileName_long;
    int _entryCnt;

public:
    DirectoryInfo(DirectoryEntryPtr entryPtr, DirectoryEntryPtr firstPtr, int entryCnt, DirectoryEntry entry, QString longFileName)
        : _entryPtr(entryPtr),_entry(entry), _fileName_long(longFileName), _firstPtr(firstPtr), _entryCnt(entryCnt) {}

    const DirectoryEntry entry() { return _entry; }
    const DirectoryEntryPtr entryPtr() { return _entryPtr; }
    const QString name() { return _fileName_long; }
    const QString shortName() { return _entry.name(); }
    const bool hasLongName() { return _entry.name() != _fileName_long; }
    const bool isReadOnly() { return _entry.isReadOnly(); }
    const bool isDirectory() { return _entry.isDirectory(); }
    const bool isVolume() { return _entry.isVolume(); }
    const quint32 firstCluster() { return _entry.firstCluster(); }
    const quint32 fileSize() { return _entry.fileSize; }
    const DirectoryEntryPtr firstPtr() { return _firstPtr; }
    const int entryCount() { return _entryCnt; }
    QString fileSizeText();
    QString fileSizeTextLong();
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

#endif // STK500_H
