#include "../stk500task.h"

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
            newStatus.append(QString::number(100 - (100 * clustersRemaining / clusterCount)));
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
        quint32 cluster = 0;
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
                timeElapsed = (time - startTime) / 1000;
                done = (fileEntry.fileSize - remaining);
                int speed_ps;
                if (timeElapsed == 0 || done == 0) {
                    speed_ps = 6000;
                } else {
                    speed_ps = done / timeElapsed;
                }

                /* Update progress */
                setProgress(progStart + progTotal * ((double) done / (double) fileEntry.fileSize));

                /* Update the status info */
                QString newStatus;
                newStatus.append("Writing ").append(destFilePath).append(": ");
                newStatus.append(stk500::getSizeText(remaining)).append(" remaining (");
                newStatus.append(stk500::getSizeText(speed_ps)).append("/s)\n");
                newStatus.append("Elapsed: ").append(stk500::getTimeText(timeElapsed));
                newStatus.append(", estimated ").append(stk500::getTimeText(remaining / speed_ps));
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
