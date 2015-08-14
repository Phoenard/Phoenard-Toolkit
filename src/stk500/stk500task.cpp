#include "stk500task.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <qmath.h>
#include "longfilenamegen.h"

#define SHOW_DOTNAMES 0

void stk500Task::showError() {
    if (hasError()) {
        QString title_lower = title();
        if (title_lower.length() > 0) {
            title_lower.replace(0, 1, title_lower.at(0).toLower());
        }
        QString message;
        message.append("An error occurred while ").append(title_lower).append(": \n\n");
        message.append(getErrorMessage());

        QMessageBox msgBox;
        msgBox.critical(0, "Phoenard Toolkit", message);
    }
}

void stk500Task::setStatus(QString status) {
    _sync.lock();
    _status.clear();
    _status.append(status);
    _sync.unlock();
}

QString stk500Task::status() {
    QString rval;
    _sync.lock();
    rval.append(_status);
    _sync.unlock();
    return rval;
}

void stk500Task::setError(ProtocolException exception) {
    _hasError = true;
    _exception = exception;
}

/* Walks by all the files using the walker, locating a requested file as needed */
QList<DirectoryInfo> stk500Task::sd_list(DirectoryEntryPtr startPtr) {
    /* If invalid, return empty list */
    if (!startPtr.isValid()) {
        QList<DirectoryInfo> empty;
        return empty;
    }

    /* Proceed to fill the list with directories using the walker */
    /* Prepare a buffer for storing the long file names */
    QList<DirectoryInfo> result;
    DirectoryEntryPtr curPtr = startPtr;
    LongFileNameGen lfn_gen;
    do {
        DirectoryEntry entry = protocol->sd().readDirectory(curPtr);
        if (entry.isFree()) break;

        if (lfn_gen.handle(curPtr, entry)) {
            QString shortName = lfn_gen.shortName();
            QString longName = lfn_gen.longName();

            // Don't add these two kind of directories
            bool isDotted = (shortName == ".") || (shortName == "..");
            if (!isDotted || SHOW_DOTNAMES) {

                // Add the found short name to the list for potentially creating later
                DirectoryInfo info(curPtr, lfn_gen.firstPtr(), lfn_gen.entryCount(), entry, longName);
                result.append(info);
            }
        }
        curPtr = protocol->sd().nextDirectory(curPtr);
    } while (!isCancelled() && curPtr.isValid());
    return result;
}

void stk500Task::sd_allocEntries(DirectoryEntryPtr startPos, int oldLength, int newLength) {
    /*
     * The goal is to resize X amount of entries into Y by shifting surrounding entries around
     * When size is increased, allocate new clusters if needed and shift the entries after to the end
     * When size is decreased, shift all entries after towards the start to fit the hole
     */
    if (newLength == oldLength) {
        // No change needed
        return;
    }
    // Reset the walker and move it to the start position requested
    DirectoryEntryPtr pos_a = startPos;
    DirectoryEntryPtr pos_b = startPos;

    // Skip the ptr ahead
    pos_a = protocol->sd().nextDirectory(pos_a, oldLength);
    pos_b = protocol->sd().nextDirectory(pos_b, newLength);

    if (newLength > oldLength) {
        // NEW > OLD - Expand
        int diff = (newLength - oldLength);

        // Start by loading in the entries between a and b into a buffer
        QList<DirectoryEntry> buffer;
        for (int i = 0; i < diff && pos_a.isValid(); i++) {
            buffer.append(protocol->sd().readDirectory(pos_a));
            pos_a = protocol->sd().nextDirectory(pos_a);
        }

        // Read the current entry at pos_b and add to the buffer, poll first and write that
        bool needsReading = pos_b.isValid();
        while (true) {
            if (needsReading) {
                DirectoryEntry entry = protocol->sd().readDirectory(pos_b);
                needsReading = !entry.isFree();
                buffer.append(entry);
                protocol->sd().writeDirectory(pos_b, buffer.takeFirst());
                pos_b = protocol->sd().nextDirectory(pos_b, 1, true);
            } else if (buffer.isEmpty()) {
                break;
            } else {
                protocol->sd().writeDirectory(pos_b, buffer.takeFirst());
            }
        }
    } else {
        // OLD > NEW - Shrink
        // pos_a is ahead of pos_b
        // Transfer entries at pos_a to pos_b
        // Increment both pointers
        while (pos_a.isValid()) {
            DirectoryEntry entry = protocol->sd().readDirectory(pos_a);
            protocol->sd().writeDirectory(pos_b, entry);
            pos_a = protocol->sd().nextDirectory(pos_a);
            pos_b = protocol->sd().nextDirectory(pos_b);
            if (entry.isFree()) {
                break;
            }
        }

        // While the current entry at pos_b is not free, wipe the entries
        while (pos_b.isValid()) {
            DirectoryEntry entry = protocol->sd().readDirectory(pos_b);
            if (entry.isFree()) {
                break;
            }
            protocol->sd().wipeDirectory(pos_b);
            pos_b = protocol->sd().nextDirectory(pos_b);
        }
    }
}

bool stk500Task::sd_remove(QString filePath, bool fileIsDir) {
    // Navigate with a walker to the directory where this file is contained
    QString fileDir = filePath;
    QString fileName;
    int dir_idx = fileDir.lastIndexOf('/');
    if (dir_idx == -1) {
        fileDir = "";
        fileName = filePath;
    } else {
        fileDir = fileDir.remove(dir_idx, fileDir.length() - dir_idx);
        fileName = filePath;
        fileName.remove(0, dir_idx + 1);
    }
    DirectoryEntryPtr dirFirstPtr;
    if (fileDir.isEmpty()) {
        dirFirstPtr = protocol->sd().getRootPtr();
    } else {
        DirectoryEntryPtr dir_ptr = sd_findEntry(fileDir, true, false);
        if (!dir_ptr.isValid()) {
            return false;
        }
        DirectoryEntry entry = protocol->sd().readDirectory(dir_ptr);
        dirFirstPtr = DirectoryEntryPtr(protocol->sd().getClusterBlock(entry.firstCluster()), 0);
    }

    QList<DirectoryInfo> allFiles = sd_list(dirFirstPtr);
    if (isCancelled()) {
        return true;
    }

    // See if the file was found, and if so, free that area by shifting entries around
    for (DirectoryInfo info : allFiles) {
        if (isCancelled()) {
            return true;
        }
        if ((info.name() == fileName) && (info.isDirectory() == fileIsDir)) {
            // File found, start by erasing the data clusters
            if (info.firstCluster()) {
                protocol->sd().wipeClusterChain(info.firstCluster());
            }
            // Proceed to erase the file entry
            sd_allocEntries(info.firstPtr(), info.entryCount(), 0);
            return true;
        }
    }
    // File not found
    return false;
}

DirectoryEntryPtr stk500Task::sd_findDirstart(QString directoryPath) {
    DirectoryEntryPtr dirEntryPtr = sd_findEntry(directoryPath, true, false);
    if (!dirEntryPtr.isValid() || (protocol->sd().getRootPtr() == dirEntryPtr)) {
        return dirEntryPtr;
    } else {
        DirectoryEntry dirEntry = protocol->sd().readDirectory(dirEntryPtr);
        return protocol->sd().getDirPtrFromCluster(dirEntry.firstCluster());
    }
}

DirectoryEntryPtr stk500Task::sd_findEntry(QString path, bool isDirectory, bool create) {
    DirectoryEntryPtr subDir = protocol->sd().getRootPtr(); // Start at ROOT

    /* Shortcut for ROOT */
    if (path.isEmpty()) {
        return subDir;
    }

    /* Navigate down each part, finding the directories as needed */
    QStringList parts = path.split('/');
    if (parts.length() > 1) {
        // Entry is located below one or more directory entries
        for (int i = 0; i < parts.length() - 1; i++) {
            // Locate the sub-directory
            subDir = sd_findEntry(subDir, parts.at(i), true, create);

            // Handle invalid/cancelled
            if (!subDir.isValid()) {
                return subDir;
            }

            // Move to the first directory pointer of the found directory
            DirectoryEntry subDirEntry = protocol->sd().readDirectory(subDir);
            subDir = protocol->sd().getDirPtrFromCluster(subDirEntry.firstCluster());
        }
    }
    /* Finally navigate to the exact file in it's sub-directory */
    return sd_findEntry(subDir, parts.at(parts.length() - 1), isDirectory, create);
}

DirectoryEntryPtr stk500Task::sd_findEntry(DirectoryEntryPtr dirStartPtr, QString name, bool isDirectory, bool create) {
    // List all files in here
    QList<DirectoryInfo> allDirectories = sd_list(dirStartPtr);
    if (isCancelled()) {
        return DirectoryEntryPtr(); // Cancelled
    }

    // Try to find the requested file among the results
    QStringList foundShortNames;
    for (DirectoryInfo dir : allDirectories) {
        if ((dir.name() == name) && (dir.isDirectory() == isDirectory)) {
            return dir.entryPtr();
        }
        foundShortNames.append(dir.shortName());
    }
    if (!create) {
        return DirectoryEntryPtr(); // File not found
    }

    // Creating is required, start at the last known entry
    DirectoryEntryPtr create_startPtr;
    if (allDirectories.isEmpty()) {
        create_startPtr = dirStartPtr;
    } else {
        DirectoryInfo lastInfo = allDirectories.at(allDirectories.length() - 1);
        create_startPtr = lastInfo.entryPtr();
    }

    // Move the start position to the first free entry
    while (true) {
        DirectoryEntry entry = protocol->sd().readDirectory(create_startPtr);
        if (entry.isFree()) {
            break;
        }
        create_startPtr = protocol->sd().nextDirectory(create_startPtr);
        if (!create_startPtr.isValid()) {
            throw ProtocolException("Directory has unexpected end");
        }
    }

    DirectoryEntryPtr resultPtr = sd_createEntry(create_startPtr, name, isDirectory, foundShortNames);
    if (resultPtr.isValid() && isDirectory) {
        // Obtain parent start cluster
        quint32 parentCluster = protocol->sd().getClusterFromBlock(dirStartPtr.block);

        // Write out the skeleton of the directory
        DirectoryEntry mainEntry = protocol->sd().readDirectory(resultPtr);

        quint32 dirCluster = protocol->sd().getClusterFromBlock(resultPtr.block);
        quint32 firstCluster = protocol->sd().findFreeCluster(dirCluster);
        quint32 firstBlock = protocol->sd().getClusterBlock(firstCluster);
        protocol->sd().fatPut(firstCluster, CLUSTER_EOC);
        mainEntry.setFirstCluster(firstCluster);

        /* Wipe the first block; then write out the '.' and '..' entries */
        protocol->sd().wipeBlock(firstBlock);

        DirectoryEntry e_dot;
        memset(&e_dot, 0, sizeof(DirectoryEntry));
        memcpy(e_dot.name_raw, ".          ", 11);
        e_dot.attributes = DIR_ATT_DIRECTORY;
        e_dot.setFirstCluster(firstCluster);
        protocol->sd().writeDirectory(DirectoryEntryPtr(firstBlock, 0), e_dot);

        DirectoryEntry e_dot_dot;
        memset(&e_dot_dot, 0, sizeof(DirectoryEntry));
        memcpy(e_dot_dot.name_raw, "..         ", 11);
        e_dot_dot.attributes = DIR_ATT_DIRECTORY;
        e_dot_dot.setFirstCluster(parentCluster);
        protocol->sd().writeDirectory(DirectoryEntryPtr(firstBlock, 1), e_dot_dot);

        protocol->sd().writeDirectory(resultPtr, mainEntry);
    }

    /* Having done all this, it's best to flush some data out */
    protocol->sd().flushCache();

    return resultPtr;
}

DirectoryEntryPtr stk500Task::sd_createEntry(DirectoryEntryPtr dirPtr, QString name, bool isDirectory, QStringList existingShortNames) {
    /*
     * Try to create the entry in the sub directory
     * Navigate the walker to the last FREE entry, write the entries starting there
     * First allocate a list of directory entries containing the file information
     * The LFN (long file name) entries are inserted before the actual file entry
     * This is only done if the file name does not fit the 8.3 short name format
     *
     * To begin, calculate the 8.3 short file name using the long name and list
     * of short names already contained in the directory. If the short name matches
     * the long name, we don't need LFN entries. If it does not match, we need
     * to create the LFN entries for the long name and use the short name in the
     * actual entry.
     */

    // Convert name to UTF8 filename, filtering illegal characters
    char extended[6] = {'+', ',', ';', '=', '[', ']'};
    QString name_utf8 = "";
    for (int i = 0; i < name.length(); i++) {
        QChar c = name.at(i).toUpper();
        ushort ucval = c.unicode();
        if (ucval < 32 || ucval >= 127) {
            continue;
        }

        // Turn extended characters into an underscore ( _ )
        for (int ex_idx = 0; ex_idx < sizeof(extended); ex_idx++) {
            if (extended[ex_idx] == c) {
                c = '_';
                break;
            }
        }
        name_utf8.append(c);
    }

    // Filter trailing dots
    while (name_utf8.length() > 0 && name_utf8.at(name_utf8.length() - 1) == '.') {
        name_utf8.remove(name_utf8.length() - 1, 1);
    }

    // Generate short name: split file name from extension
    QString name_base = "";
    QString name_ext = "";
    int ext_idx = name_utf8.lastIndexOf('.') + 1;
    if (ext_idx) {
        // Generate extension
        int len = name_utf8.length() - ext_idx;
        for (int i = 0; i < len; i++) {
            name_ext.append(name_utf8.at(ext_idx + i));
        }
        // Generate base file name excluding the extension, filter all dots ('.')
        for (int i = 0; i < ext_idx - 1; i++) {
            QChar c = name_utf8.at(i);
            if (c != '.') {
                name_base.append(c);
            }
        }
    } else {
        // No (empty) extension, only filename
        // Not dots ('.') found earlier, so no further checks needed
        name_base.append(name_utf8);
    }

    // Remove spaces at first if name/ext violates basic rules
    if (name_base.length() > 8 || name_ext.length() > 3 || name_ext.contains(' ')) {
        name_base.remove(' ');
    }
    name_ext.remove(' ');

    // Trim base name to a length of at most 8 characters
    if (name_base.length() > 8) {
        name_base.remove(8, name_base.length() - 8);
    }

    // Now, combine the base and extension and check if it's still the same name
    // If so, congratz! No special LFN entries needed. If not, more work is needed...
    QString shortName = "";
    shortName.append(name_base);
    if (name_ext.length() > 0) {
        shortName.append('.').append(name_ext);
    }
    bool needsLFN = (shortName != name) && (shortName.toLower() != name);
    if (needsLFN) {
        // Filter spaces in all cases, if we haven't already
        shortName.remove(' ');

        // Further generate the short name using ~ and name hash
        if (name_base.length() > 2) {
            // Get base name limited to 6 characters
            QString tmpName = name_base;
            if (tmpName.length() > 6) {
                tmpName.remove(6, tmpName.length() - 6);
            }

            // ~[1..4] short name generating
            shortName = sd_findShortName(tmpName, name_ext, existingShortNames);
        }

        // If no short name found this run, generate one using a hash of the name
        if (shortName.isEmpty()) {
            // Get base name limited to 2 characters
            QString tmpName = name_base;
            if (tmpName.length() > 2) {
                tmpName.remove(2, tmpName.length() - 2);
            }

            // Append a 4-char long hex number generated from the original file name
            // No documentation on this was found, but it's not that important.
            // Here we used the exact same algorithm as used for the 8.3 checksum hash
            quint16 nameHash = 0;
            for (int i = 0; i < name.length(); i++) {
                nameHash = ((nameHash<<15) | (nameHash>>1)) + name.at(i).unicode();
            }
            QString nameHashText = QString::number(nameHash, 16).toUpper();
            while (nameHashText.length() < 4) {
                nameHashText.insert(0, '0');
            }

            // Finally append the hash text
            tmpName.append(nameHashText);

            // ~[1..4] short name generating
            shortName = sd_findShortName(tmpName, name_ext, existingShortNames);
        }

        // Still not found?! Error I guess...
        if (shortName.isEmpty()) {
            throw ProtocolException("Could not find valid 8.3 short name to use");
        }
    }

    /* Prepare the main directory entry first */
    DirectoryEntry mainEntry;
    memset(&mainEntry, 0, sizeof(DirectoryEntry));
    mainEntry.setName(shortName);
    if (isDirectory) {
        mainEntry.attributes = DIR_ATT_DIRECTORY | DIR_ATT_READ_ONLY;
    } else {
        mainEntry.attributes = 0;
    }

    /* If name is turned from uppercase to lowercase, set that attribute */
    /* Note: experimental */
    if ((shortName != name) && (shortName.toLower() == name)) {
        mainEntry.reservedNT |= 0x18;
    } else {
        mainEntry.reservedNT &= ~0x18;
    }

    /* Generate all the entries to write out */
    DirectoryEntry entriesToWrite[25];
    int entryCount = 0;
    if (needsLFN) {
        // Proceed to write LFN entries using the directory walker
        // This stores the original UTF16 filename

        // Convert the name to UTF16, with a modulus 13 size blocks
        int name_utf16_blocks = qCeil((qreal) (name.length() + 1) / 13);
        int name_utf16_len = 13 * name_utf16_blocks;
        ushort *name_utf16 = new ushort[name_utf16_len];
        for (int i = 0; i < name_utf16_len; i++) {
            ushort c;
            if (i < name.length()) {
                // Add name character
                c = name.at(i).unicode();
            } else if (i == name.length()) {
                // Add null terminator
                c = 0x0000;
            } else {
                // After the null terminator, append with 0xFFFF
                c = 0xFFFF;
            }
            name_utf16[i] = c;
        }

        // Store the short name CRC
        unsigned char shortNameCRC = mainEntry.name_crc();

        // Generate the LFN entries to write out
        DirectoryEntry* name_entries = new DirectoryEntry[name_utf16_blocks];
        for (unsigned char i = 0; i < name_utf16_blocks; i++) {
            // Create a new entry, initialize to NULL then fill in details
            DirectoryEntry entry;
            memset(&entry, 0, sizeof(DirectoryEntry));
            entry.LFN_writeName(name_utf16, i);
            entry.LFN_setOrdinal(i, i == (name_utf16_blocks - 1));
            entry.attributes = DIR_ATT_LFN;
            entry.LFN_setChecksum(shortNameCRC);
            // Add to the result array
            name_entries[i] = entry;
        }

        /* Write out the LFN entries, last one first */
        int i = name_utf16_blocks;
        while (i--) {
            entriesToWrite[entryCount++] = name_entries[i];
        }
    }
    entriesToWrite[entryCount++] = mainEntry;

    // Start off by calculating how many entries to overwrite
    DirectoryEntryPtr tmpPtr = dirPtr;
    int oldCount = 0;
    do {
        DirectoryEntry entry = protocol->sd().readDirectory(tmpPtr);
        if (entry.isFree()) {
            break;
        }
        oldCount++;
        if (!entry.isDeleted() && (entry.isFile() || entry.isDirectory())) {
            break;
        }
        tmpPtr = protocol->sd().nextDirectory(tmpPtr);
    } while (tmpPtr.isValid());

    // Allocate the memory needed to write the entry
    sd_allocEntries(dirPtr, oldCount, entryCount);

    // Write out the entries at the space freed
    DirectoryEntryPtr mainEntryPtr;
    for (int i = 0; i < entryCount; i++) {
        mainEntryPtr = dirPtr;
        protocol->sd().writeDirectory(dirPtr, entriesToWrite[i]);
        dirPtr = protocol->sd().nextDirectory(dirPtr, 1, true);
    }

    // All done! Return the pointer to the current entry
    return mainEntryPtr;
}

/*
 * Appends ~[1..4] to the base name and appends the extension to try and generate a short name
 * If all four possibilities already exist in the list specified, an empty String is returned instead
 */
QString stk500Task::sd_findShortName(QString name_base, QString name_ext, QStringList existingShortNames) {
    QString shortName = "";
    int num_idx;

    // Generate the short name base
    shortName.append(name_base);
    shortName.append("~0");
    num_idx = shortName.length() - 1;
    if (!name_ext.isEmpty()) {
        shortName.append('.').append(name_ext);
    }

    // Check short names ~[1..4]
    for (char c = '1'; c <= '4'; c++) {
        shortName.replace(num_idx, 1, c);

        if (!existingShortNames.contains(shortName)) {
            return shortName;
        }
    }

    // Not found
    return "";
}

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

void stk500SaveFiles::saveFile(DirectoryEntry fileEntry, QString sourceFilePath, QString destFilePath, double progStart, double progTotal) {
    /* Ensure that the parent directory exists */
    QString destFolderPath = destFilePath;
    int destFolderIdx = destFolderPath.lastIndexOf('/');
    if (destFolderIdx != -1) {
        destFolderPath.remove(destFolderIdx, destFolderPath.length() - destFolderIdx);
    }
    QDir dir = QDir::root();
    dir.mkpath(destFolderPath);

    /* Open the file for writing */
    QFile destFile(destFilePath);
    if (!destFile.open(QIODevice::WriteOnly)) {
        throw ProtocolException("Failed to open file for writing");
    }

    /* Proceed to read in data */
    quint32 cluster = fileEntry.firstCluster();
    if (cluster) {
        char buff[512];
        quint32 remaining = fileEntry.fileSize;
        quint32 done = 0;
        qint64 startTime = QDateTime::currentMSecsSinceEpoch();
        qint64 time = startTime;
        qint64 timeElapsed = 0;
        while (remaining > 0) {
            quint32 block = protocol->sd().getClusterBlock(cluster);
            for (int i = 0; i < protocol->sd().volume().blocksPerCluster; i++) {
                protocol->sd().read(block + i, 0, buff, 512);

                /* If cancelled, stop reading/writing by setting remaining to 0 */
                if (isCancelled()) {
                    remaining = 0;
                }

                time = QDateTime::currentMSecsSinceEpoch();
                timeElapsed = (time - startTime);
                done = (fileEntry.fileSize - remaining);
                int speed_ps = 1000 * done / timeElapsed;
                if (!speed_ps) speed_ps = 1;

                /* Update progress */
                setProgress(progStart + progTotal * ((double) done / (double) fileEntry.fileSize));

                /* Update the status info */
                QString newStatus;
                newStatus.append("Reading ").append(sourceFilePath).append(": ");
                newStatus.append(stk500::getSizeText(remaining)).append(" remaining (");
                newStatus.append(stk500::getSizeText(speed_ps)).append("/s)\n");
                newStatus.append("Elapsed: ").append(stk500::getTimeText(timeElapsed));
                newStatus.append(", estimated ").append(stk500::getTimeText(1000 * remaining / speed_ps));
                newStatus.append(" remaining");
                setStatus(newStatus);

                /* Write the 512 or less bytes of buffered data to the file */
                if (remaining < 512) {
                    destFile.write(buff, remaining);
                    remaining = 0;
                    break;
                } else {
                    destFile.write(buff, 512);
                    remaining -= 512;
                }
            }

            // Next cluster, if end of chain no more clusters follow
            cluster = protocol->sd().fatGet(cluster);
            if (protocol->sd().isEOC(cluster)) {
                break;
            }
        }
    }

    // If errors occur the deconstructor closes it as well...
    destFile.close();

    // If cancelled, delete the file again (awh...)
    if (isCancelled()) {
        destFile.remove();
    }
}

void stk500SaveFiles::saveFolder(DirectoryEntryPtr dirStartPtr, QString sourceFilePath, QString destFilePath, double progStart, double progTotal) {
    // First of all - create the destination directory before processing
    QDir dir(destFilePath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Perform a file listing of the current directory
    QList<DirectoryInfo> subFiles = sd_list(dirStartPtr);

    // Filter any VOLUME entries
    int tmpIdx = 0;
    while (tmpIdx < subFiles.length()) {
        DirectoryInfo info = subFiles.at(tmpIdx);
        if (info.isVolume()) {
            subFiles.removeAt(tmpIdx);
        } else {
            tmpIdx++;
        }
    }

    // If cancelled or nothing to go through, stop here
    if (isCancelled() || subFiles.isEmpty()) {
        return;
    }

    // Name to append before the filename to produce a source location
    QString srcPath = sourceFilePath;
    if (!srcPath.isEmpty()) {
        srcPath.append('/');
    }

    // Go by all the files processing them
    int totalFiles = subFiles.length();
    double subProgTotal = progTotal * (1.0 / (double) totalFiles);
    for (int fileIdx = 0; fileIdx < totalFiles && !isCancelled(); fileIdx++) {
        DirectoryInfo info = subFiles.at(fileIdx);
        QString subSrcPath = srcPath + info.name();
        QString subDestPath = destFilePath + '/' + info.name();
        double subProgStart = progStart + progTotal * ((double) fileIdx / (double) totalFiles);
        if (info.isDirectory()) {
            DirectoryEntryPtr dirStartPtr = protocol->sd().getDirPtrFromCluster(info.firstCluster());
            saveFolder(dirStartPtr, subSrcPath, subDestPath, subProgStart, subProgTotal);
        } else {
            saveFile(info.entry(), subSrcPath, subDestPath, subProgStart, subProgTotal);
        }
    }
}

void stk500SaveFiles::run() {
    if (this->sourceFile.endsWith('/')) {
        // Saving a full directory
        // First navigate to this directory
        QString dirPath = this->sourceFile;
        dirPath.remove(dirPath.length() - 1, 1);
        QString destDirPath = this->destFile;
        if (destDirPath.endsWith('/')) {
            destDirPath.remove(destDirPath.length() - 1, 1);
        }

        DirectoryEntryPtr dirStartPtr;
        if (dirPath.isEmpty()) {
            dirStartPtr = protocol->sd().getRootPtr();
        } else {
            DirectoryEntryPtr dirEntryPtr = sd_findEntry(dirPath, true, false);
            if (isCancelled()) {
                return;
            }
            if (!dirEntryPtr.isValid()) {
                // Should not happen, but just in case...
                throw ProtocolException("Folder not found");
            }
            DirectoryEntry folderEntry = protocol->sd().readDirectory(dirEntryPtr);
            if (folderEntry.firstCluster()) {
                dirStartPtr = protocol->sd().getDirPtrFromCluster(folderEntry.firstCluster());
            } else {
                dirStartPtr = DirectoryEntryPtr(0, 0);
            }
        }
        saveFolder(dirStartPtr, dirPath, destDirPath, 0.0, 1.0);
    } else {
        // Saving a single file
        DirectoryEntryPtr filePtr = sd_findEntry(this->sourceFile, false, false);
        if (isCancelled()) {
            return;
        }
        if (!filePtr.isValid()) {
            throw ProtocolException("File not found");
        }
        DirectoryEntry fileEntry = protocol->sd().readDirectory(filePtr);
        saveFile(fileEntry, this->sourceFile, this->destFile, 0.0, 1.0);
    }
}

void stk500ImportFiles::importFile(DirectoryEntryPtr dirStartPtr, QString sourceFilePath, QString destFilePath, double progStart, double progTotal) {
    /* Parse file name from the destination path */
    QString fileName = destFilePath;
    fileName.remove(0, fileName.lastIndexOf('/') + 1);

    /* Find or create the file entry */
    DirectoryEntryPtr filePtr = sd_findEntry(dirStartPtr, fileName, false, true);
    if (isCancelled()) {
        return;
    }
    if (!filePtr.isValid()) {
        // Should not happen, but just in case...
        throw ProtocolException("Failed to create file");
    }

    /* Store the found or created file entry */
    destEntry = protocol->sd().readDirectory(filePtr);

    /* Proceed to read in the source file */

    // Open the source file for reading, if needed that is
    QFile sourceFile(sourceFilePath);
    if (!this->sourceFile.isEmpty() && !sourceFile.open(QIODevice::ReadOnly)) {
        throw ProtocolException("Failed to open file for reading");
    }

    // Set up the dest file entry information
    DirectoryEntry fileEntry = destEntry;
    if (sourceFile.isOpen()) {
        fileEntry.fileSize = sourceFile.size();
    } else {
        fileEntry.fileSize = 0;
    }

    // Calculate how many clusters will be needed to store the file's contents
    int clusterCount = qCeil((qreal) fileEntry.fileSize / (qreal) (512 * protocol->sd().volume().blocksPerCluster));

    // Pre-allocate the clusters
    if (clusterCount) {
        // If no first cluster allocated, do that first
        quint32 cluster = fileEntry.firstCluster();
        if (!cluster) {
            cluster = protocol->sd().findFreeCluster(protocol->sd().getClusterFromBlock(filePtr.block));
            fileEntry.setFirstCluster(cluster);
        }

        // Go down the chain to check if enough clusters are allocated
        quint32 cluster_next;
        int clustersRemaining = clusterCount;
        while (--clustersRemaining) {
            cluster_next = protocol->sd().fatGet(cluster);
            if (protocol->sd().isEOC(cluster_next)) {
                cluster_next = protocol->sd().findFreeCluster(cluster);
                protocol->sd().fatPut(cluster, cluster_next);
            }
            cluster = cluster_next;

            // Update status
            QString newStatus = "";
            newStatus.append("Allocating file contents (");
            newStatus.append(QString::number(100 * clustersRemaining / clusterCount));
            newStatus.append("%)");
            setStatus(newStatus);
        }

        // If current cluster has further chains, delete them
        cluster_next = protocol->sd().fatGet(cluster);
        if (!protocol->sd().isEOC(cluster_next)) {
            protocol->sd().wipeClusterChain(cluster_next);
        }
        protocol->sd().fatPut(cluster, CLUSTER_EOC);
    } else {
        // Wipe all clusters if specified
        if (fileEntry.firstCluster() != 0) {
            protocol->sd().wipeClusterChain(fileEntry.firstCluster());
            fileEntry.setFirstCluster(0);
        }
    }

    // With the start entry prepared, write it out right now
    // This also flushes out any pending FAT writes
    protocol->sd().writeDirectory(filePtr, fileEntry);
    protocol->sd().flushCache();

    bool hasReadError = false;
    if (fileEntry.fileSize) {
        /* Proceed to write out the data, this stuff could fail any moment... */
        char buff[512];
        quint32 remaining = fileEntry.fileSize;
        quint32 done = 0;
        qint64 startTime = QDateTime::currentMSecsSinceEpoch();
        qint64 time = startTime;
        qint64 timeElapsed = 0;

        /* Prepare a buffer for storing the clusters */
        const int cluster_buffer_len = 256;
        quint32 cluster_buffer[cluster_buffer_len];
        quint32 cluster_remaining = 0;
        bool isFirstCluster = true;
        quint32 cluster;
        while (remaining > 0) {

            if (!cluster_remaining) {
                if (isFirstCluster) {
                    isFirstCluster = false;
                    cluster = fileEntry.firstCluster();
                    cluster_buffer[cluster_remaining++] = cluster;
                }

                quint32 cluster_next = cluster;
                while (cluster_remaining < cluster_buffer_len) {
                    cluster_next = protocol->sd().fatGet(cluster_next);
                    if (protocol->sd().isEOC(cluster_next)) {
                        break;
                    } else {
                        cluster_buffer[cluster_remaining++] = cluster_next;
                    }
                }
            }
            if (!cluster_remaining) {
                // No more clusters available (odd?)
                throw ProtocolException("Ran out of clusters to write to (Allocation error)");
            }

            // Poll the first cluster from the top of the buffer
            cluster = cluster_buffer[0];
            cluster_remaining--;
            memcpy(cluster_buffer, cluster_buffer + 1, cluster_remaining * sizeof(quint32));

            quint32 block = protocol->sd().getClusterBlock(cluster);
            for (int i = 0; i < protocol->sd().volume().blocksPerCluster; i++) {
                int read = sourceFile.read(buff, 512);
                if (read == -1) {
                    hasReadError = true;
                    remaining = 0;
                } else if (read < 512) {
                    remaining = read;
                }

                /* If cancelled, stop reading/writing by setting remaining to 0 */
                if (isCancelled()) {
                    remaining = 0;
                }

                time = QDateTime::currentMSecsSinceEpoch();
                timeElapsed = (time - startTime);
                if (!timeElapsed) timeElapsed = 1;
                done = (fileEntry.fileSize - remaining);
                int speed_ps = 1000 * done / timeElapsed;
                if (!speed_ps) speed_ps = 1;

                /* Update progress */
                setProgress(progStart + progTotal * ((double) done / (double) fileEntry.fileSize));

                /* Update the status info */
                QString newStatus;
                newStatus.append("Writing ").append(destFilePath).append(": ");
                newStatus.append(stk500::getSizeText(remaining)).append(" remaining (");
                newStatus.append(stk500::getSizeText(speed_ps)).append("/s)\n");
                newStatus.append("Elapsed: ").append(stk500::getTimeText(timeElapsed));
                newStatus.append(", estimated ").append(stk500::getTimeText(1000 * remaining / speed_ps));
                newStatus.append(" remaining");
                setStatus(newStatus);

                /* Write the full block of data to the Micro-SD */
                protocol->sd().write(block + i, 0, buff, 512);
                if (remaining < 512) {
                    remaining = 0;
                    break;
                } else {
                    remaining -= 512;
                }
            }
        }
    }

    /* Flush data to the Micro-SD */
    protocol->sd().flushCache();

    /* Close eventually - is also done by the deconstructor when errors occur */
    if (sourceFile.isOpen()) {
        sourceFile.close();
    }

    /* If cancelled or an error occurred while reading; delete the file */
    if (isCancelled() || hasReadError) {
        suppressCancel(true);
        sd_remove(this->destFile, false);
        suppressCancel(false);
    }

    /* Deal with this once all is well and done... */
    if (hasReadError) {
        throw ProtocolException("An error occurred while reading source file");
    }
}

void stk500ImportFiles::importFolder(DirectoryEntryPtr dirStartPtr, QString sourceFilePath, QString destFilePath, double progStart, double progTotal) {
    /* Parse folder name from the destination path */
    QString fileName = stk500::getFileName(destFilePath);

    /* Find or create the folder entry */
    DirectoryEntryPtr folderPtr = sd_findEntry(dirStartPtr, fileName, true, true);
    if (isCancelled()) {
        return;
    }
    if (!folderPtr.isValid()) {
        // Should not happen, but just in case...
        throw ProtocolException("Failed to create folder");
    }

    /* Store the found or created entry */
    destEntry = protocol->sd().readDirectory(folderPtr);

    /* Check if the source folder exists at all */
    if (sourceFilePath.isEmpty()) {
        return;
    }

    /* List all files and folders inside */
    QDir folderDir(sourceFilePath);
    QFileInfoList subFiles = folderDir.entryInfoList(QDir::NoDot | QDir::NoDotAndDotDot | QDir::NoDotDot | QDir::NoSymLinks | QDir::AllEntries);
    if (subFiles.isEmpty()) {
        return;
    }

    /* Read the directory first cluster */
    DirectoryEntry folderEntry = destEntry;
    DirectoryEntryPtr folderStartPtr = protocol->sd().getDirPtrFromCluster(folderEntry.firstCluster());

    /* Start processing all the files/folders inside */
    double progStep = progTotal * (1.0 / (double) subFiles.length());
    for (int i = 0; i < subFiles.length(); i++) {
        /* Gather the required information to process the file or folder */
        QFileInfo subFile = subFiles.at(i);
        double progSubStart = progStart + (double) i * progStep;
        QString subSourcePath = subFile.absoluteFilePath();
        QString subDestPath = destFilePath + '/' + subFile.fileName();
        if (subFile.isDir()) {
            importFolder(folderStartPtr, subSourcePath, subDestPath, progSubStart, progStep);
        } else {
            importFile(folderStartPtr, subSourcePath, subDestPath, progSubStart, progStep);
        }
    }
}

void stk500ImportFiles::run() {
    bool isDirectory = false;
    QString sourcePath = this->sourceFile;
    QString destPath = this->destFile;
    if (sourcePath.endsWith('/')) {
        sourcePath.remove(sourcePath.length() - 1, 1);
        isDirectory = true;
    }
    if (destPath.endsWith('/')) {
        destPath.remove(destPath.length() - 1, 1);
        isDirectory = true;
    }

    // Find directory where this file has to be added
    DirectoryEntryPtr rootPtr;
    int destDirIdx = destPath.lastIndexOf('/');
    if (destDirIdx == -1) {
        // ROOT
        rootPtr = protocol->sd().getRootPtr();
    } else {
        // Sub-directory, create it
        QString destDirPath = destPath;
        destDirPath.remove(destDirIdx, destDirPath.length() - destDirIdx);
        DirectoryEntryPtr eptr = sd_findEntry(destDirPath, true, true);
        if (isCancelled()) {
            return;
        }
        if (!eptr.isValid()) {
            throw ProtocolException("Folder not found");
        }
        // Read root ptr
        DirectoryEntry entry = protocol->sd().readDirectory(eptr);
        rootPtr = protocol->sd().getDirPtrFromCluster(entry.firstCluster());
    }
    if (isDirectory) {
        importFolder(rootPtr, sourcePath, destPath, 0.0, 1.0);
    } else {
        importFile(rootPtr, sourcePath, destPath, 0.0, 1.0);
    }
}

void stk500Test::run() {
    QThread::msleep(2000);
}

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

void stk500ListSketches::run() {
    /* Read the currently loaded sketch from EEPROM */
    currentSketch = protocol->readSettings().getCurrent();

    /* List all file entries in the root directory */
    QList<DirectoryInfo> rootEntries = sd_list(protocol->sd().getDirPtrFromCluster(0));
    if (isCancelled()) return;

    /* Go by all entries and find all icons, or use the defaults */
    for (int entryIdx = 0; entryIdx < rootEntries.length(); entryIdx++) {
        DirectoryInfo &info = rootEntries[entryIdx];
        DirectoryEntry entry = info.entry();
        QString shortName = info.shortName();
        QString shortExt = stk500::getFileExt(shortName);
        bool isIcon = (shortExt == "SKI");

        /* Update current progress */
        setProgress((double) entryIdx / (double) rootEntries.length());

        /* Check if this entry is a HEX or SKI file */
        if (info.isDirectory() || (!isIcon && (shortExt != "HEX"))) {
            continue;
        }

        /* Generate a temporary entry for comparison and adding */
        SketchInfo sketch;
        sketch.name = stk500::trimFileExt(info.shortName());
        sketch.fullName = stk500::trimFileExt(info.name());
        sketch.hasIcon = false;
        sketch.iconDirty = true;
        sketch.iconBlock = 0;

        /* Locate this entry in the current results, or add if not found */
        int sketchIndex = -1;
        for (int i = 0; i < sketches.length(); i++) {
            SketchInfo &other = sketches[i];
            if (sketch.name == other.name) {
                sketchIndex = i;
                sketch = other;
                break;
            }
        }
        if (sketchIndex == -1) {
            sketchIndex = sketch.index = sketches.length();
            sketches.append(sketch);
        }

        /* If icon, load the icon data */
        quint32 firstCluster = entry.firstCluster();
        if (isIcon) {
            if (firstCluster == 0) {
                sketch.iconBlock = 0;
            } else {
                sketch.iconBlock = protocol->sd().getClusterBlock(firstCluster);
            }
        }

        /* Update entry */
        sketches[sketchIndex] = sketch;
    }

    /* Completed */
    setProgress(1.0);
}

void stk500LaunchSketch::run() {
    PHN_Settings settings = protocol->readSettings();
    if (settings.getCurrent() != sketchName) {
        settings.setToload(sketchName);
        settings.flags |= SETTINGS_LOAD;
        protocol->writeSettings(settings);
        protocol->resetDelayed();
    }
}

void stk500LoadIcon::run() {
    if (sketch.iconBlock == 0) {
        sketch.setIcon(SKETCH_DEFAULT_ICON);
    } else {
        sketch.setIcon(protocol->sd().cacheBlock(sketch.iconBlock, true, false, false));
    }
    sketch.iconDirty = false;
    sketch.hasIcon = true;
}
