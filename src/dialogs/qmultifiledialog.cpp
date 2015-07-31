#include "qmultifiledialog.h"

QMultiFileDialog::QMultiFileDialog() : QFileDialog() {
    m_btnOpen = NULL;
    m_listView = NULL;
    m_treeView = NULL;
    m_selectedFiles.clear();

    this->setOption(QFileDialog::DontUseNativeDialog, true);
    this->setOption(QFileDialog::DontResolveSymlinks, true);
    this->setFileMode(QFileDialog::Directory);
    QList<QPushButton*> btns = this->findChildren<QPushButton*>();
    for (int i = 0; i < btns.size(); ++i) {
        QString text = btns[i]->text();
        if (text.toLower().contains("open") || text.toLower().contains("choose"))
        {
            m_btnOpen = btns[i];
            break;
        }
    }

    if (!m_btnOpen) return;

    m_btnOpen->installEventFilter(this);
    m_btnOpen->disconnect(SIGNAL(clicked()));
    connect(m_btnOpen, SIGNAL(clicked()), this, SLOT(chooseClicked()));


    m_listView = findChild<QListView*>("listView");
    if (m_listView) {
        m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

    m_treeView = findChild<QTreeView*>();
    if (m_treeView) {
        m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

}

bool QMultiFileDialog::eventFilter( QObject* watched, QEvent* event ) {
    QPushButton *btn = qobject_cast<QPushButton*>(watched);
    if (btn) {
        if(event->type()==QEvent::EnabledChange) {
            if (!btn->isEnabled()) {
                btn->setEnabled(true);
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void QMultiFileDialog::chooseClicked() {
    QModelIndexList indexList = m_listView->selectionModel()->selectedIndexes();
    QString lastPath = "";
    foreach (QModelIndex index, indexList) {
        if (index.column() == 0) {
            if (!lastPath.isEmpty()) {
                m_selectedFiles.append(lastPath);
            }
            lastPath = this->directory().absolutePath() + '/' + index.data().toString();
        } else if (index.column() == 1) {
            // Check if a file size is specified. If not, it's a directory.
            if (!lastPath.isEmpty() && index.data().toString().isEmpty()) {
                lastPath.append('/');
            }
        }
    }
    if (!lastPath.isEmpty()) {
        m_selectedFiles.append(lastPath);
    }
    QDialog::accept();
}

QStringList QMultiFileDialog::selectedFiles() {
    return m_selectedFiles;
}
