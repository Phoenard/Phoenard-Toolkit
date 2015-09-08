#include "../stk500task.h"

void stk500ListSubDirs::run() {
    quint32 firstCluster = 0;
    if (!directoryPath.isEmpty()) {
        // Sub-directories
        DirectoryEntryPtr dirPtr = sd_findEntry(directoryPath, true, false);
        if (!dirPtr.isValid()) {
            // Folder not found - return empty list
            return;
        }
        DirectoryEntry entry = protocol->sd().readDirectory(dirPtr);
        firstCluster = entry.firstCluster();
    }
    result = sd_list(protocol->sd().getDirPtrFromCluster(firstCluster));
}