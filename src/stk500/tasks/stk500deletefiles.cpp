#include "../stk500task.h"

void stk500Delete::run() {
    // Locate the directory the file is contained in
    DirectoryEntryPtr parentDirStart = sd_findDirstart(directoryPath);
    if (!parentDirStart.isValid()) {
        throw ProtocolException("File(s) parent directory not found");
    }

    // Delete the requested files in the found directory tree
    deleteInFolder(parentDirStart, fileNames, false, true);
}

void stk500Delete::deleteInFolder(DirectoryEntryPtr dirStartPtr, QStringList fileNames, bool all, bool storeFiles) {
    DirectoryEntryPtr curPtr = dirStartPtr;
    DirectoryEntryPtr writePtr = dirStartPtr;
    LongFileNameGen lfn_gen;
    while (true) {
        DirectoryEntry entry = protocol->sd().readDirectory(curPtr);
        if (entry.isFree()) break;

        // Volume labels need to be written at all times
        if (entry.isVolume() && !entry.isLFN()) {
            protocol->sd().writeDirectory(writePtr, entry);
            writePtr = protocol->sd().nextDirectory(writePtr);
            curPtr = protocol->sd().nextDirectory(curPtr);
            continue;
        }

        if (lfn_gen.handle(curPtr, entry)) {
            QString longName = lfn_gen.longName();

            // If not cancelling and file name is marked for deletion; delete it
            bool isDeleted = false;
            bool isDotted = (longName == ".") || (longName == "..");
            if (!isCancelled() && (!isDotted || SHOW_DOTNAMES) && (all || fileNames.removeOne(longName))) {
                // This entry needs to be removed; delete using the right methods
                if (!entry.firstCluster()) {
                    // No data, we can wipe easily
                    isDeleted = true;
                } else {
                    // Got data, need to wipe that first...
                    if (entry.isDirectory()) {
                        DirectoryEntryPtr ptr = protocol->sd().getDirPtrFromCluster(entry.firstCluster());
                        QStringList empty;
                        deleteInFolder(ptr, empty, true, false);

                        // Not cancelled; we did it!
                        isDeleted = !isCancelled();
                    } else {
                        isDeleted = true;
                    }
                    if (isDeleted) {
                        protocol->sd().wipeClusterChain(entry.firstCluster());
                    }
                }
            }

            // Finally; if deleted, simply skip writing the entry
            // Otherwise, write all entries to the current position to write to
            if (!isDeleted) {
                int e_entryCount = lfn_gen.entryCount();
                DirectoryEntryPtr e_firstPtr = lfn_gen.firstPtr();

                // Shift-write the entries; skip if pointers are the same
                DirectoryEntryPtr e_curPtr = e_firstPtr;
                while (e_entryCount--) {
                    if (writePtr == e_curPtr) {
                        e_curPtr = writePtr = protocol->sd().nextDirectory(writePtr);
                    } else {
                        protocol->sd().writeDirectory(writePtr, protocol->sd().readDirectory(e_curPtr));
                        writePtr = protocol->sd().nextDirectory(writePtr);
                        e_curPtr = protocol->sd().nextDirectory(e_curPtr);
                    }
                }

                // If listing enabled, store a directory info about this entry
                if (storeFiles && (longName != ".") && (longName != "..")) {
                    DirectoryInfo info(curPtr, e_firstPtr, e_entryCount, entry, longName);
                    currentFiles.append(info);
                }
            }
        }
        curPtr = protocol->sd().nextDirectory(curPtr, true);
    }

    // curPtr stores the pointer to the last entry read in
    // writePtr stores the pointer to the last entry written + 1
    // While the writePtr is lower than curPtr, wipe the blocks
    // If a new cluster is opened, we can wipe the entire cluster chain
    quint32 oldCluster = protocol->sd().getClusterFromBlock(writePtr.block);
    while (writePtr != curPtr) {
        // Wipe the block
        protocol->sd().wipeDirectory(writePtr);
        writePtr = protocol->sd().nextDirectory(writePtr);

        // If new write ptr is a different cluster; wipe the entire cluster chain
        quint32 newCluster = protocol->sd().getClusterFromBlock(writePtr.block);
        if (newCluster != oldCluster) {
            protocol->sd().wipeClusterChain(newCluster);
            protocol->sd().fatPut(oldCluster, CLUSTER_EOC);
            break;
        }
    }
}
