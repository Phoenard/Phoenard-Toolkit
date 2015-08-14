#ifndef STK500SD_H
#define STK500SD_H

#include "stk500.h"
#include <QDebug>

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
class stk500sd
{
public:
    stk500sd(stk500 *handler);
    void init(bool forceInit = false);
    void reset();
    void debugPrint();
    CardVolume volume();

    /* Cache handling */
    char* cacheBlock(quint32 block, bool readBlock, bool markDirty, bool isFat = false);
    void flushCache();
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

private:
    void writeOutCache(BlockCache *cache);

    stk500 *_handler;
    CardVolume _volume;
    BlockCache _cache[SD_CACHE_CNT];
};

// Stores all information needed to access a file or directory
class DirectoryInfo {
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

private:
    DirectoryEntryPtr _firstPtr;
    DirectoryEntryPtr _entryPtr;
    DirectoryEntry _entry;
    QString _fileName_long;
    int _entryCnt;
};

#endif // STK500SD_H
