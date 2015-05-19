#ifndef LONGFILENAMEGEN_H
#define LONGFILENAMEGEN_H

#include "stk500_fat.h"
#include <QString>

#define LFN_BUFFER_LENGTH 300

class LongFileNameGen
{
public:
    LongFileNameGen();
    void clear();
    bool handle(DirectoryEntryPtr entryPtr, DirectoryEntry entry);
    const QString longName() { return _longName; }
    const QString shortName() { return _shortName; }
    const DirectoryEntryPtr firstPtr() { return _firstPtr; }
    const int entryCount() { return _entryCount; }
private:
    ushort longFileName[LFN_BUFFER_LENGTH];
    uchar longFileName_crc;
    DirectoryEntryPtr _firstPtr;
    int _entryCount;
    QString _longName;
    QString _shortName;
    bool _clearNext;
};

#endif // LONGFILENAMEGEN_H
