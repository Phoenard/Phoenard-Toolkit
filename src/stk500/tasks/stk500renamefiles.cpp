#include "../stk500task.h"

void stk500Rename::run() {
    // Obtain old file name and whether it's a directory; trim /
    QString oldFileName = filePath;
    bool fileIsDir = filePath.endsWith('/');
    if (fileIsDir) {
        oldFileName.remove(oldFileName.length() - 1, 1);
    }

    // Navigate and list the directory where the file/directory is contained
    QString parentDir = "";
    int dirIdx = oldFileName.lastIndexOf('/');
    if (dirIdx != -1) {
        parentDir = oldFileName;
        parentDir.remove(dirIdx, parentDir.length() - dirIdx);
        oldFileName.remove(0, dirIdx + 1);
    }
    // If name unchanged, don't do anything
    if (oldFileName == newFileName) {
        return;
    }
    // Navigate to the directory
    DirectoryEntryPtr parentDirPtr = sd_findDirstart(parentDir);
    if (isCancelled()) {
        return;
    }
    if (!parentDirPtr.isValid()) {
        throw ProtocolException("File parent directory not found");
    }
    // List all files in this directory
    QList<DirectoryInfo> subFiles = sd_list(parentDirPtr);
    QStringList existingShortNames;
    DirectoryEntry oldFileEntry;
    DirectoryEntryPtr oldFileFirstPtr;
    bool newNameTaken = false;
    if (isCancelled()) {
        return;
    }
    for (DirectoryInfo info : subFiles) {
        if (fileIsDir == info.isDirectory()) {
            if (info.name() == oldFileName) {
                oldFileFirstPtr = info.firstPtr();
                oldFileEntry = info.entry();

                // Don't add the short name
                continue;
            } else if (info.name() == newFileName) {
                newNameTaken = true;
            }
        }
        existingShortNames.append(info.shortName());
    }
    if (!oldFileFirstPtr.isValid()) {
        isMissing = true;
        if (ignoreMissing) return;
        throw ProtocolException("File not found in directory");
    }
    if (newNameTaken) {
        throw ProtocolException("New file name already taken");
    }
    DirectoryEntryPtr newPtr = sd_createEntry(oldFileFirstPtr, newFileName, fileIsDir, existingShortNames);

    // Write the original entry properties to the new entry
    DirectoryEntry oldEntry = protocol->sd().readDirectory(newPtr);
    DirectoryEntry newEntry = oldFileEntry;
    memcpy(newEntry.name_raw, oldEntry.name_raw, 11);
    newEntry.reservedNT = (newEntry.reservedNT & ~0x18) | (oldEntry.reservedNT & 0x18);
    protocol->sd().writeDirectory(newPtr, newEntry);
}