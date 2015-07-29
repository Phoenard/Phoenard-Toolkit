#include "longfilenamegen.h"
#include <qDebug>

LongFileNameGen::LongFileNameGen() {
    clear();
}

void LongFileNameGen::clear() {
    memset(longFileName, 0, sizeof(ushort) * LFN_BUFFER_LENGTH);
    _entryCount = 0;
    _firstPtr = DirectoryEntryPtr(0, 0);
    longFileName_crc = 0;
    _clearNext = false;
    _shortName = "";
    _longName = "";
}

bool LongFileNameGen::handle(DirectoryEntryPtr entryPtr, DirectoryEntry entry) {
    // Ignore deleted/free entries
    if (entry.isDeleted()) return false;
    if (entry.isFree()) return false;

    // Clear if specified
    if (_clearNext) {
        clear();
    }

    // First entry?
    if (!_entryCount) {
        _firstPtr = entryPtr;
        _entryCount = 0;
    }
    _entryCount++;

    if (entry.isLFN()) {
        // Reset on CRC mismatch
        if (entry.LFN_checksum() != longFileName_crc) {
            clear();
            longFileName_crc = entry.LFN_checksum();
            _firstPtr = entryPtr;
            _entryCount = 1;
        }
        // Update long name
        entry.LFN_readName(longFileName, entry.LFN_ordinal());
        // No valid data yet; false
        return false;
    } else {
        _shortName = entry.name();
        _longName = _shortName;

        // Calculate the length of the long file name
        int len = 0;
        for (;len < LFN_BUFFER_LENGTH && longFileName[len]; len++);

        // If stored in the buffer; read it
        // Check that the short name CRC matches
        if (len && (entry.name_crc() == longFileName_crc)) {
            _longName = QString::fromUtf16(longFileName, len);
        } else {
            // Note: this stuff is very experimental...no idea if it's correct
            // It won't do harm though...the long name is 'virtual' so to say
            quint8 NT_LOWER = 0x18;
            if ((entry.reservedNT & NT_LOWER) == NT_LOWER) {
                _longName = _longName.toLower();
            }
        }

        // Tell it to clear the next time an entry comes in
        _clearNext = true;

        // We got valid data!
        return true;
    }
}
