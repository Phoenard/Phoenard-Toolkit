#include "sdbrowserwidget.h"
#include <QDebug>
#include <QMimeData>
#include <QMessageBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QDesktopServices>
#include "../dialogs/qmultifiledialog.h"

const QString DEFAULT_VOLUME_NAME = "SD";

SDBrowserWidget::SDBrowserWidget(QWidget *parent)
    : QTreeWidget(parent) {

    this->setColumnWidth(0, 300);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setEditTriggers(EditKeyPressed | SelectedClicked);
    this->setExpandsOnDoubleClick(true);

    // Slots
    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)), SLOT(on_itemExpanded(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemChanged(QTreeWidgetItem*,int)), SLOT(on_itemChanged(QTreeWidgetItem*,int)));
    connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), SLOT(on_itemDoubleClicked(QTreeWidgetItem*,int)));

    // Default variables
    _isRefreshing = false;
}

void SDBrowserWidget::on_itemDoubleClicked(QTreeWidgetItem *item, int column) {
    if (!itemIsFolder(item) && column >= 0) {
        QString sourcePath = getPath(item);
        if (!sourcePath.isEmpty()) {
            // Generate the temporary file to write to
            QString destPath = stk500::getTempFile(sourcePath);

            // Read the file
            stk500SaveFiles saveTask(sourcePath, destPath);
            serial->execute(saveTask);
            if (!saveTask.isCancelled() && !saveTask.hasError()) {
                QDesktopServices::openUrl(QUrl(destPath));
            }
        }
    }
}

void SDBrowserWidget::on_itemChanged(QTreeWidgetItem *item, int column) {
    if (column > 0 || !(item->flags() & Qt::ItemIsEditable)) {
        return;
    }
    QString oldText = item->data(0, Qt::UserRole).toString();
    QString newText = item->text(0);
    if (oldText == newText) {
        return;
    }
    QChar illegalChars[9] = {'?', '<', '>', '\\', ':', '*', '|', '"', '^'};
    bool isVolume = (item->parent() == NULL);
    bool success = false;
    for (QChar illegalChar : illegalChars) {
        newText.remove(illegalChar);
    }
    if (isVolume) {
        // First limit length to 11 characters
        if (newText.length() > 11) {
            newText.remove(11, newText.length() - 11);
        }
        // Then filter extended chars and make others uppercase
        char extended[6] = {'+', ',', ';', '=', '[', ']'};
        for (int i = 0; i < newText.length(); i++) {
            char c = newText.at(i).toUpper().toLatin1();
            for (int exIdx = 0; exIdx < sizeof(extended); exIdx++) {
                if (c == extended[exIdx]) {
                    c = '_';
                    break;
                }
            }
            if (c >= 127 || c < 32) {
                c = '_';
            }
            newText.replace(i, 1, c);
        }
    }

    if (oldText == newText) {
        return;
    }

    if (isVolume) {
        // Renaming the volume label
        stk500RenameVolume renameTask(newText);
        serial->execute(renameTask);
        success = !renameTask.hasError();
    } else if (!newText.isEmpty()) {
        // Renaming file or folder
        QString filePath = getPath(item->parent());
        if (!filePath.isEmpty()) {
            filePath.append('/');
        }
        filePath.append(oldText);
        if (itemIsFolder(item)) {
            filePath.append('/');
        }

        // Start renaming
        stk500Rename renameTask(filePath, newText);
        serial->execute(renameTask);
        success = !renameTask.hasError();
    }

    // Update the item accordingly
    if (success) {
        item->setData(0, Qt::UserRole, qVariantFromValue(newText));
        if (item->text(0) != newText) {
            item->setText(0, newText);
        }
        updateItemIcon(item);
    } else {
        item->setText(0, oldText);
    }
}

void SDBrowserWidget::updateItemIcon(QTreeWidgetItem *item) {
    QIcon icon;
    if (item->childIndicatorPolicy() == QTreeWidgetItem::ShowIndicator) {
        if ((topLevelItemCount() == 0) || (item == topLevelItem(0))) {
            // Drive
            icon = iconProvider.getDriveIcon();
        } else {
            // Folder
            icon = iconProvider.getFolderIcon();
        }
    } else {
        // File
        icon = iconProvider.getIcon(item->text(0));
    }
    item->setIcon(0, icon);
}

void SDBrowserWidget::dragEnterEvent(QDragEnterEvent *event) {
    event->acceptProposedAction();
    qDebug() << "ENTER";
}

void SDBrowserWidget::dragMoveEvent(QDragMoveEvent *event) {
    event->acceptProposedAction();
}

void SDBrowserWidget::dropEvent(QDropEvent *event) {
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        QStringList filePaths;
        QString text;
        for (int i = 0; i < urlList.size() && i < 32; ++i) {
            QString url = urlList.at(i).path();
            if (url.startsWith('/')) {
                url = url.remove(0, 1);
            }
            filePaths.append(url);
            qDebug() << url;
        }
        importFiles(filePaths);
    } else {
        qDebug() << "UNKNOWN STUFF";
    }
    setBackgroundRole(QPalette::Dark);
    event->acceptProposedAction();
}

void SDBrowserWidget::dragLeaveEvent(QDragLeaveEvent *event) {
    event->accept();
    qDebug() << "LEAVE";
}

void SDBrowserWidget::keyPressEvent(QKeyEvent *ev) {
    QTreeWidget::keyPressEvent(ev);

    switch (ev->key()) {
    case Qt::Key_Delete:
        deleteFiles();
        break;
    case Qt::Key_F5:
        refreshFiles();
        break;
    default:
        break;
    }
}

void SDBrowserWidget::importFiles() {
    // Find out what destination folder we are importing to
    QTreeWidgetItem *folderItem = this->getSelectedFolder();
    if (!folderItem) {
        return;
    }

    // Ask user to select the files to import
    QMultiFileDialog dialog;
    dialog.setWindowTitle("Select files and directories to import into '" + folderItem->text(0) + "'");
    if (dialog.exec()) {
        importFiles(dialog.selectedFiles(), getPath(folderItem));
    }
}

void SDBrowserWidget::importFiles(QStringList filePaths) {
    // Find out what destination folder we are importing to
    QTreeWidgetItem *folderItem = this->getSelectedFolder();
    if (!folderItem) {
        return;
    }

    // Import the files
    importFiles(filePaths, getPath(folderItem));
}

void SDBrowserWidget::importFiles(QStringList filePaths, QString destFolder) {
    // Generate a list of tasks to execute
    QList<stk500Task*> tasks;
    for (QString sourceFile : filePaths) {
        QString destFile = destFolder;
        if (!destFile.isEmpty()) {
            destFile.append('/');
        }
        destFile.append(stk500::getFileName(sourceFile));
        tasks.append(new stk500ImportFiles(sourceFile, destFile));
    }
    // Execute them all
    serial->executeAll(tasks);
    // Delete them
    for (stk500Task *task : tasks) {
        delete task;
    }
    // Refresh
    refreshFiles();
}

void SDBrowserWidget::createNew(QString fileName, bool isDirectory) {
    QTreeWidgetItem *folderItem = this->getSelectedFolder();
    if (folderItem != NULL) {
        QString destPath = getPath(folderItem);
        if (!destPath.isEmpty()) {
            destPath.append('/');
        }
        destPath.append(fileName);
        if (isDirectory) {
            destPath.append('/');
        }

        // Create file or directory
        stk500ImportFiles importTask("", destPath);
        serial->execute(importTask);
        this->refreshItem(folderItem);

        QTreeWidgetItem *newItem = this->getItemAtPath(destPath);
        if (newItem != NULL) {
            this->scrollToItem(newItem);
            this->editItem(newItem);
        }
    }
}

QTreeWidgetItem* SDBrowserWidget::getSelectedFolder() {
    QTreeWidgetItem* foundFolder = NULL;
    for (QTreeWidgetItem *item : this->selectedItems()) {
        if (itemHasSelectedParent(item)) {
            continue;
        }
        QTreeWidgetItem *folder;
        if (itemIsFolder(item)) {
            folder = item;
        } else {
            folder = item->parent();
        }
        if ((folder == NULL) || (folder == foundFolder)) {
            continue;
        }
        if (foundFolder == NULL) {
            foundFolder = folder;
        } else {
            // Multiple selected - return NULL
            return NULL;
        }
    }
    return foundFolder;
}

bool SDBrowserWidget::hasSelection() {
    return !this->selectedItems().isEmpty();
}

QTreeWidgetItem *SDBrowserWidget::getItemAtPath(QString path) {
    if (this->topLevelItemCount() == 0) {
        return NULL;
    }
    QTreeWidgetItem *parent = this->topLevelItem(0);
    if (path.isEmpty() || path == "/") {
        return parent;
    }
    bool pathIsDir = path.endsWith('/');
    if (pathIsDir) {
        path.remove(path.length() - 1, 1);
    }
    while (true) {
        // Take first part in path and find that node
        // Proceed next until path is empty or node not found
        QString part = path;
        int endIdx = part.indexOf('/');
        if (endIdx == -1) {
            path = "";
        } else {
            part.remove(endIdx, part.length() - endIdx);
            path.remove(0, endIdx + 1);
        }

        // Locate the node in the current parent
        bool found = false;
        bool partIsDir = pathIsDir || (!path.isEmpty());
        for (int i = 0; i < parent->childCount(); i++) {
            QTreeWidgetItem *child = parent->child(i);
            // Check if the child is a correct file/directory
            if (itemIsFolder(child) != partIsDir) {
                continue;
            }
            // Compare name
            if (child->text(0) == part) {
                found = true;
                parent = child;
                break;
            }
        }

        // If found and end achieved, return it
        if (!found) {
            return NULL;
        }
        if (path.isEmpty()) {
            return parent;
        }
    }
}

void SDBrowserWidget::addExpandedListTasks(QTreeWidgetItem *item, QList<stk500Task*> *tasks) {
    if (item->isExpanded()) {
        tasks->append(new stk500ListSubDirs(getPath(item)));
        for (int i = 0; i < item->childCount(); i++) {
            addExpandedListTasks(item->child(i), tasks);
        }
    }
}

void SDBrowserWidget::refreshFiles() {
    QList<stk500Task*> listTasks;

    // Just in case
    _isRefreshing = true;

    // If no root item added, do that first
    if (this->topLevelItemCount() == 0) {
        QTreeWidgetItem *rootItem = new QTreeWidgetItem;
        rootItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        setupItemName(rootItem, DEFAULT_VOLUME_NAME);
        this->addTopLevelItem(rootItem);
        rootItem->setExpanded(true);
    }

    // First generate a list of expanded folder nodes
    // For each expanded node an additional list instruction is required
    for (int i = 0; i < this->topLevelItemCount(); i++) {
        addExpandedListTasks(this->topLevelItem(i), &listTasks);
    }

    // Process the list command
    serial->executeAll(listTasks);

    // Fill the Tree widget with the results
    for (int i = 0; i < listTasks.length(); i++) {
        stk500ListSubDirs *task = (stk500ListSubDirs*) listTasks.at(i);
        QTreeWidgetItem *item = getItemAtPath(task->directoryPath + '/');
        if (item != NULL && !task->isCancelled()) {
            refreshItem(item, task->result);
        }
        delete task;
    }

    // All done
    _isRefreshing = false;
}

void SDBrowserWidget::refreshItem(QTreeWidgetItem *item) {
    // Already refreshing, don't you dare!
    stk500ListSubDirs listTask(getPath(item));
    serial->execute(listTask);

    // Fill the item with the resulting directories
    refreshItem(item, listTask.result);
}

void SDBrowserWidget::refreshItem(QTreeWidgetItem *item, QList<DirectoryInfo> subFiles) {
    // Already refreshing, don't you dare!
    _isRefreshing = true;

    // Generate a list of 'old' items
    QList<QTreeWidgetItem*> oldItems;
    // Wipe old contents
    for (int i = 0; i < item->childCount(); i++) {
        oldItems.append(item->child(i));
    }

    // Expand the item all silently...
    item->setExpanded(true);

    // Fill with new contents
    for (DirectoryInfo directory : subFiles) {
        if (directory.isVolume()) {
            setupItemName(item, directory.name());
            continue;
        }
        bool isFolder = directory.isDirectory();
        bool isItemFound = false;
        QString itemName = directory.name();
        QTreeWidgetItem *childItem = NULL;

        // Find the item in the old list
        for (int i = 0; i < oldItems.length(); i++) {
            QTreeWidgetItem *oldItem = oldItems.at(i);
            if ((itemIsFolder(oldItem) == isFolder) && (oldItem->text(0) == itemName)) {
                isItemFound = true;
                childItem = oldItem;
                oldItems.removeAt(i);
                break;
            }
        }

        // Create a new item if needed
        if (!isItemFound) {
            childItem = new QTreeWidgetItem;
            childItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsSelectable);
        }

        // Calculate and apply item tooltip information
        QString toolTip;
        toolTip.append("Short name: ").append(directory.shortName());
        if (!isFolder) {
            toolTip.append("\nFile size: ").append(directory.fileSizeTextLong());
            if (directory.isReadOnly()) {
                toolTip.append("\nThis file is read-only");
            }
        }
        childItem->setToolTip(0, toolTip);
        childItem->setToolTip(1, toolTip);

        // If it is a folder, apply property
        if (isFolder) {
            childItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }

        // Apply name and file size text to the item
        setupItemName(childItem, itemName);
        if (isFolder) {
            childItem->setText(1, "");
        } else {
            childItem->setText(1, directory.fileSizeText());
        }

        // If not found, we need to add it
        if (!isItemFound) {
            if (isFolder) {
                // Directory - append to the end of the last directory
                int idx = 0;
                for (idx = 0; idx < item->childCount(); idx++) {
                    if (!itemIsFolder(item->child(idx))) {
                        break;
                    }
                }
                item->insertChild(idx, childItem);
            } else {
                // Append to the end
                item->addChild(childItem);
            }
        }
    }

    // Any items remaining in the list: delete them!
    for (QTreeWidgetItem *removed : oldItems) {
        item->removeChild(removed);
    }

    // All done!
    _isRefreshing = false;
}

void SDBrowserWidget::setupItemName(QTreeWidgetItem *item, QString name) {
    Qt::ItemFlags flags = item->flags();
    flags &= ~Qt::ItemIsEditable;
    item->setFlags(flags);
    item->setData(0, Qt::UserRole, qVariantFromValue(name));
    item->setText(0, name);
    flags |= Qt::ItemIsEditable;
    item->setFlags(flags);
    updateItemIcon(item);
}

QString SDBrowserWidget::getPath(QTreeWidgetItem *item) {
    if (item->parent() == NULL) return "";

    QString text;
    QTreeWidgetItem *current = item;
    int rootLength = 0;
    while (current != NULL) {
        rootLength = current->text(0).length();
        text.insert(0, '/');
        text.insert(1, current->text(0));
        current = current->parent();
    }
    // Trim starting /
    if (!text.isEmpty()) {
        text.remove(0, 1);
    }
    // Trim root
    if (text.length() > (rootLength + 1)) {
        text.remove(0, rootLength + 1);
    }
    // Done!
    return text;
}

void SDBrowserWidget::saveFilesTo() {
    QStringList sourceFiles;
    QString rootPath = "";
    QString folderName = "";
    bool isCopyingRoot = false;
    for (QTreeWidgetItem* item : this->selectedItems()) {
        // Ignore items with selected parent nodes
        if (itemHasSelectedParent(item)) {
            continue;
        }

        // Copying the root item includes everything on the Micro-SD
        if (item->parent() == NULL) {
            isCopyingRoot = true;
            break;
        }

        // Gather a list of file paths to copy
        // At the same time obtain the lowest path
        QString itemPath = getPath(item);
        if (rootPath != "/") {
            QString itemDir = itemPath;
            if (itemIsFolder(item)) {
                // Item is a folder - append / to path
                itemDir.append('/');
            } else {
                // Item is a file - obtain directory + /
                int itemPathDirIdx = itemDir.lastIndexOf('/');
                if (itemPathDirIdx == -1) {
                    itemDir = "";
                } else {
                    itemDir.remove(itemPathDirIdx + 1, itemDir.length() - itemPathDirIdx - 1);
                }
            }

            // If no directory, set root to "/"
            if (itemDir.isEmpty()) {
                rootPath = "/";
            } else if (rootPath.isEmpty()) {
                // No previous root set; set directly
                rootPath = itemDir;
            } else {
                // Update root directory path
                // ROOT: Images/Dir1/
                // NEW:  Images/Dir2/
                // Must become: Images/
                // New: NewFolder/
                // Must become: /
                // Reduce ROOT PATH until it fits in itemDir
                while (!itemDir.startsWith(rootPath)) {

                    // Remove last folder from ROOT path
                    // If last name is discarded, it is in ROOT (/)
                    int lastDirIdx = rootPath.lastIndexOf('/', rootPath.length() - 2);
                    if (lastDirIdx == -1) {
                        rootPath = "/";
                        break;
                    }
                    rootPath.remove(lastDirIdx + 1, rootPath.length() - lastDirIdx - 1);
                }
            }
        }

        // Add file to the list of source files
        // If a directory, append a / first to denote it as such
        if (itemIsFolder(item)) {
            itemPath.append('/');
        }
        sourceFiles.append(itemPath);
    }

    if (isCopyingRoot || rootPath.isEmpty() || (rootPath == "/")) {
        // Select the SD volume name as the folder name
        folderName = volumeName();
    } else {
        // Select last element of the root path as the folder name
        folderName = rootPath;
        if (folderName.endsWith('/')) {
            folderName.remove(folderName.length() - 1, 1);
        }
        int lastIdx = folderName.lastIndexOf('/');
        if (lastIdx != -1) {
            folderName.remove(0, lastIdx + 1);
        }
    }

    bool isCopyingMany = isCopyingRoot || (sourceFiles.length() > 1);
    if (!isCopyingMany && sourceFiles.at(0).endsWith('/')) {
        QString folderPath = sourceFiles.at(0);
        folderPath.remove(folderPath.length() - 1, 1);
        isCopyingMany = true;

        // Find filename and use as folder name
        folderName = folderPath;
        folderName.remove(0, folderName.lastIndexOf('/') + 1);
    }

    // Tasks to be executed
    QList<stk500Task*> tasks;

    // Check if we are copying multiple files
    if (isCopyingMany) {
        // Copying all files on the Micro-SD, allow the user to select the destination folder
        // This folder is saved as the original volume name

        QFileDialog fileDialog;
        fileDialog.setParent(this);
        fileDialog.setFileMode(QFileDialog::Directory);
        fileDialog.setOptions(QFileDialog::ShowDirsOnly);
        if (sourceFiles.length() == 1) {
            fileDialog.setWindowTitle("Select destination folder to save '" + folderName + "' to");
        } else {
            fileDialog.setWindowTitle("Select destination folder for all files in '" + folderName + "'");
            fileDialog.selectFile(folderName);
        }

        if (fileDialog.exec()) {
            QString destFolder = fileDialog.selectedFiles().at(0);
            if (isCopyingRoot) {
                // Set source path to / and use the destination folder
                tasks.append(new stk500SaveFiles("/", destFolder + "/"));
            } else if (sourceFiles.length() == 1) {
                // Copy a single directory
                QString sourceFilePath = sourceFiles.at(0);
                QString destFilePath = destFolder + '/' + stk500::getFileName(sourceFilePath) + '/';
                tasks.append(new stk500SaveFiles(sourceFilePath, destFilePath));
            } else {
                // Copy all files/directories specified to the destination directory
                for (QString filePath : sourceFiles) {
                    QString destFile = filePath;
                    if (rootPath != "/") {
                        // In a sub-folder, cut root path from the filePath
                        destFile.remove(0, rootPath.length());
                    }
                    // Put the destination folder at the beginning
                    destFile.insert(0, '/').insert(0, destFolder);

                    // Finally, add the task
                    tasks.append(new stk500SaveFiles(filePath, destFile));
                }
            }
        }
    }

    // Copying a single file
    if (!isCopyingMany && (sourceFiles.length() == 1)) {
        QString sourceFile = sourceFiles.at(0);

        // Find filename
        QString fileName = sourceFile;
        fileName.remove(0, sourceFile.lastIndexOf('/') + 1);

        QString destFile = QFileDialog::getSaveFileName(this, "Select destination location", fileName);
        if (!destFile.isEmpty()) {
            tasks.append(new stk500SaveFiles(sourceFile, destFile));
        }
    }

    // Finally execute any tasks
    serial->executeAll(tasks);

    // Delete the tasks again
    for (stk500Task* task : tasks) {
        delete task;
    }
}

void SDBrowserWidget::deleteFiles() {
    // Find all selected nodes and their fully qualified paths, and delete/refresh
    // Do this while selected items remain
    QList<QTreeWidgetItem*> itemsToProcess;
    QList<stk500Task*> tasks;
    QList<QTreeWidgetItem*> taskItems;
    bool hasDirectories = false;
    int itemCount = 0;
    QString lastFilename;
    while (true) {
        QList<QTreeWidgetItem*> selectedItems = this->selectedItems();
        for (QTreeWidgetItem* toProcess : itemsToProcess) {
            selectedItems.removeAll(toProcess);
        }
        if (selectedItems.isEmpty()) {
            break;
        }
        // Take the first item, first verify one of it's parent items is not selected already
        QTreeWidgetItem *first = selectedItems.at(0);
        if (itemHasSelectedParent(first)) {
            // Already being processed; ignore and retry
            itemsToProcess.append(first);
            continue;
        }

        // Look at the current item's parent and list all selected children
        // If no parent, then it must be root...special use case?
        QTreeWidgetItem *dir = first->parent();
        if (dir) {
            // Build up a list of files to delete
            QStringList subFiles;
            for (int i = 0; i < dir->childCount(); i++) {
                QTreeWidgetItem *item = dir->child(i);
                if (item->isSelected()) {
                    itemsToProcess.append(item);
                    subFiles.append(item->text(0));
                    itemCount++;
                    lastFilename = item->text(0);

                    if (itemIsFolder(item)) {
                        hasDirectories = true;
                    }
                }
            }
            // Proceed to delete all files in the subFiles list in the sub-directory
            tasks.append(new stk500Delete(getPath(dir), subFiles));
            taskItems.append(dir);
        } else {
            //ROOT can not be deleted. Nope.
            itemsToProcess.append(first);
        }
    }
    if (tasks.isEmpty()) {
        return;
    }

    // Ask user for confirmation, just in case he made a terrible mistake
    // Build up a message
    QString messageToUser = "";
    if (itemCount == 1) {
        messageToUser.append("Are you sure you want to delete '");
        messageToUser.append(lastFilename).append("'");
    } else {
        messageToUser.append("Are you sure you want to delete all ");
        messageToUser.append(QString::number(itemCount));
        messageToUser.append(" items");
    }
    if (hasDirectories) {
        messageToUser.append(" and the sub-directories?");
    } else {
        messageToUser.append("?");
    }

    // Ask
    QMessageBox message(this);
    int result = message.warning(this, "Deleting", messageToUser, QMessageBox::Yes, QMessageBox::No);
    if (result == QMessageBox::Yes) {

        // Process all the tasks
        serial->executeAll(tasks);

        // Gather the results
        for (int i = 0; i < taskItems.count(); i++) {
            stk500Delete *deleteTask = (stk500Delete*) tasks.at(i);
            QTreeWidgetItem *dirItem = taskItems.at(i);
            if (!deleteTask->hasError()) {
                // Refresh the tree widget with the updated contents
                refreshItem(dirItem, deleteTask->currentFiles);
            }
        }
    }

    // Delete the entries
    for (int i = 0; i < tasks.count(); i++) {
        delete tasks.at(i);
    }
    tasks.clear();
}

bool SDBrowserWidget::itemHasSelectedParent(QTreeWidgetItem *item) {
    QTreeWidgetItem *parent = item->parent();
    while (parent != NULL) {
        if (parent->isSelected()) {
            return true;
        }
        parent = parent->parent();
    }
    return false;
}

bool SDBrowserWidget::itemIsFolder(QTreeWidgetItem *item) {
    return item->childIndicatorPolicy() == QTreeWidgetItem::ShowIndicator;
}

QString SDBrowserWidget::volumeName() {
    if (topLevelItemCount()) {
        return topLevelItem(0)->text(0);
    } else {
        return DEFAULT_VOLUME_NAME;
    }
}

void SDBrowserWidget::on_itemExpanded(QTreeWidgetItem *item) {
    if (!_isRefreshing) {
        refreshItem(item);
    }
}
