#include "../stk500task.h"

void stk500RenameVolume::run() {
    DirectoryEntryPtr firstPtr = protocol->sd().getRootPtr();
    DirectoryEntryPtr curPtr = firstPtr;

    // Find the first volume entry
    do {
        DirectoryEntry entry = protocol->sd().readDirectory(curPtr);
        if (entry.isFree()) {
            curPtr = DirectoryEntryPtr(0, 0);
            break;
        }
        if (!entry.isDeleted() && entry.isVolume() && !entry.isLFN()) {
            break;
        }
        curPtr = protocol->sd().nextDirectory(curPtr);
    } while (curPtr.isValid());

    // If found and marked empty - delete the entry
    if (curPtr.isValid() && volumeName.isEmpty()) {
        sd_allocEntries(curPtr, 1, 0);
        return;
    }

    // If not found, create it
    if (!curPtr.isValid()) {
        curPtr = firstPtr;
        sd_allocEntries(curPtr, 0, 1);

        DirectoryEntry volumeEntry;
        memset(&volumeEntry, 0, sizeof(DirectoryEntry));
        volumeEntry.attributes = DIR_ATT_VOLUME_ID;
        protocol->sd().writeDirectory(curPtr, volumeEntry);
    }

    // Read entry and update the name
    DirectoryEntry volumeEntry = protocol->sd().readDirectory(curPtr);
    volumeEntry.setName(volumeName);
    protocol->sd().writeDirectory(curPtr, volumeEntry);
}