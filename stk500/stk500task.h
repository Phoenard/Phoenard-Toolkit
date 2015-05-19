#ifndef STK500TASK_H
#define STK500TASK_H

#include "stk500.h"
#include <QStringList>
#include <QList>

class stk500Task
{
public:
    stk500Task(QString title = "") : _hasError(false), _isCancelled(false), _progress(-1.0), _status(title + "..."), _title(title), _cancelSuppress(false) {}
    virtual void run() = 0;
    void setProtocol(stk500 protocol) { this->protocol = protocol; }
    const bool hasError() { return _hasError; }
    const bool isCancelled() { return (_isCancelled && !_cancelSuppress); }
    void suppressCancel(bool suppress) { _cancelSuppress = suppress; }
    const bool isCancelSuppressed() { return _cancelSuppress; }
    const ProtocolException getError() { return _exception; }
    const char* getErrorMessage() { return _exception.what(); }
    void setError(ProtocolException exception);
    void cancel() { _isCancelled = true; }
    void setProgress(double progress) { _progress = progress; }
    const double progress() { return _progress; }
    void setStatus(QString status);
    void showError();
    QString status();
    QString title() { return _title; }

    /* Helpful utility functions task implementations can use */
    DirectoryEntryPtr sd_findEntry(QString path, bool isDirectory, bool create);
    DirectoryEntryPtr sd_findEntry(DirectoryEntryPtr dirStartPtr, QString name, bool isDirectory, bool create);
    DirectoryEntryPtr sd_findDirstart(QString directoryPath);
    DirectoryEntryPtr sd_createEntry(DirectoryEntryPtr dirPtr, QString name, bool isDirectory, QStringList existingShortNames);
    QString sd_findShortName(QString name_base, QString name_ext, QStringList existingShortNames);

    QList<DirectoryInfo> sd_list(DirectoryEntryPtr startPtr);
    void sd_allocEntries(DirectoryEntryPtr startPos, int oldLength, int newLength);
    bool sd_remove(QString fileName, bool fileIsDir);
    DirectoryEntryPtr sd_findOrCreate(DirectoryWalker walker, QString fileName);
    DirectoryEntryPtr sd_rename(DirectoryWalker walker, QString oldName, QString newName);
protected:
    stk500 protocol;

private:
    ProtocolException _exception;
    bool _hasError;
    bool _isCancelled;
    bool _cancelSuppress;
    double _progress;
    QString _title;
    QString _status;
    QMutex _sync;
};

class stk500ListSubDirs : public stk500Task {
public:
    stk500ListSubDirs(QString directoryPath) : stk500Task("Listing files"), directoryPath(directoryPath) {}
    virtual void run();

    QString directoryPath;
    QList<DirectoryInfo> result;
};

class stk500SaveFiles : public stk500Task {
public:
    stk500SaveFiles(QString sourceFile, QString destFile) : stk500Task("Reading from device"), sourceFile(sourceFile), destFile(destFile) {}
    virtual void run();
    void saveFile(DirectoryEntry fileEntry, QString sourceFilePath, QString destFilePath, double progStart, double progTotal);
    void saveFolder(DirectoryEntryPtr dirStartPtr, QString sourceFilePath, QString destFilePath, double progStart, double progTotal);

    QString sourceFile;
    QString destFile;
};

class stk500ImportFiles : public stk500Task {
public:
    stk500ImportFiles(QString sourceFile, QString destFile) : stk500Task("Writing to device"), sourceFile(sourceFile), destFile(destFile) {}
    virtual void run();
    void importFile(DirectoryEntryPtr dirStartPtr, QString sourceFilePath, QString destFilePath, double progStart, double progTotal);
    void importFolder(DirectoryEntryPtr dirStartPtr, QString sourceFilePath, QString destFilePath, double progStart, double progTotal);

    QString sourceFile;
    QString destFile;
};

/*
 * Deletes all file names specified in the fileNames list
 * It then returns a new list of directories remaining in the directory
 */
class stk500Delete : public stk500Task {
public:
    stk500Delete(QString directoryPath, QStringList fileNames)
        : stk500Task("Deleting file"), directoryPath(directoryPath), fileNames(fileNames) {}
    virtual void run();
    void deleteInFolder(DirectoryEntryPtr dirStartPtr, QStringList fileNames, bool all, bool storeFiles);

    QString directoryPath;
    QStringList fileNames;
    QList<DirectoryInfo> currentFiles;
};

class stk500Rename : public stk500Task {
public:
    stk500Rename(QString filePath, QString newFileName) : stk500Task("Renaming file"), filePath(filePath), newFileName(newFileName) {}
    virtual void run();

    QString filePath;
    QString newFileName;
    QList<DirectoryInfo> currentFiles;
};

class stk500RenameVolume : public stk500Task {
public:
    stk500RenameVolume(QString volumeName) : stk500Task("Renaming volume"), volumeName(volumeName) {}
    virtual void run();

    QString volumeName;
};

class stk500Test : public stk500Task {
public:
    stk500Test() : stk500Task("Testing") {}
    virtual void run();
};

#endif // STK500TASK_H
