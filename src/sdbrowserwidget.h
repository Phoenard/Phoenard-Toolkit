#ifndef SDBROWSERWIDGET_H
#define SDBROWSERWIDGET_H

#include <QTreeWidget>
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include "stk500/stk500serial.h"
#include "mainmenutab.h"

class SDBrowserWidget : public QTreeWidget, public MainMenuTab
{
    Q_OBJECT

public:
    SDBrowserWidget(QWidget *parent = 0);
    void refreshFiles();
    void deleteFiles();
    void saveFilesTo();
    void importFiles();
    void importFiles(QStringList filePaths);
    void importFiles(QStringList filePaths, QString destFolder);
    void createNew(QString fileName, bool isDirectory);
    void keyPressEvent(QKeyEvent *ev);
    bool hasSelection();
    QTreeWidgetItem* getSelectedFolder();
    QString volumeName();
protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void dropEvent(QDropEvent *event);
    void itemExpanded(QTreeWidgetItem *item);
    void refreshItem(QTreeWidgetItem *item);
    void refreshItem(QTreeWidgetItem *item, QList<DirectoryInfo> subFiles);
    void setupItemName(QTreeWidgetItem *item, QString name);
    void addExpandedListTasks(QTreeWidgetItem *item, QList<stk500Task*> *tasks);
    QString getPath(QTreeWidgetItem *item);
    QTreeWidgetItem *getItemAtPath(QString path);
    bool itemHasSelectedParent(QTreeWidgetItem *item);
    bool itemIsFolder(QTreeWidgetItem *item);

private slots:
    void on_itemExpanded(QTreeWidgetItem *item);
    void on_itemChanged(QTreeWidgetItem *item, int column);
    void on_itemDoubleClicked(QTreeWidgetItem *item, int column);

private:
    bool _isRefreshing;
};

#endif // SDBROWSERWIDGET_H
