#ifndef FORMATSELECTDIALOG_H
#define FORMATSELECTDIALOG_H

#include <QDialog>
#include "../controls/imageviewer.h"

namespace Ui {
class FormatSelectDialog;
}

class FormatSelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FormatSelectDialog(QWidget *parent = 0);
    ~FormatSelectDialog();
    void setData(ImageViewer *editor, QByteArray &data);

private slots:
    void on_fmtBox_activated(int index);

    void on_resBox_currentIndexChanged(int index);

private:
    Ui::FormatSelectDialog *ui;
    QByteArray *data;
    ImageViewer *editor;
    QList<QSize> resolutions[6];
};

#endif // FORMATSELECTDIALOG_H
