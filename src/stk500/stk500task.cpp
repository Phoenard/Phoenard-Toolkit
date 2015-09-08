#include "stk500task.h"

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

/***************************************************************
 ************* Standard Micro-SD Task Functions ****************
 ***************************************************************
 *
 * Standard Micro-SD navigation and FAT16/FAT32 traversing logic
 * is implemented here for use by other tasks. This makes it
 * easier to create very specific tasks that deal with the
 * fairly complex file system.
 *
 * All functions supported here handle task cancelling and error
 * handling fully. If something goes wrong, the filesystem will
 * be restored to the best ability. If the user decides to cancel
 * the current operation, any ongoing task (listing, modifying)
 * is stopped. It is up to the caller to properly check for an
 * active cancel state after the function returns.
 */

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

