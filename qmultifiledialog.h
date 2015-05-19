#ifndef QMULTIFILEDIALOG_H
#define QMULTIFILEDIALOG_H

#include <QListView>
#include <QTreeView>
#include <QPushButton>
#include <QFileDialog>
#include <QEvent>

class QMultiFileDialog : public QFileDialog
{
    Q_OBJECT

public slots:
    void chooseClicked();

public:
    QMultiFileDialog();
    QStringList selectedFiles();
    bool eventFilter(QObject* watched, QEvent* event);

private:
    QListView *m_listView;
    QTreeView *m_treeView;
    QPushButton *m_btnOpen;
    QStringList m_selectedFiles;
};

#endif // QMULTIFILEDIALOG_H
