#include "../stk500task.h"

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
                newStatus.append("Reading ").append(sourceFilePath).append(": ");
                newStatus.append(stk500::getSizeText(remaining)).append(" remaining (");
                newStatus.append(stk500::getSizeText(speed_ps)).append("/s)\n");
                newStatus.append("Elapsed: ").append(stk500::getTimeText(timeElapsed));
                newStatus.append(", estimated ").append(stk500::getTimeText(remaining / speed_ps));
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
