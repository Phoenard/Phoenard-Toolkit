#ifndef ICONEDITDIALOG_H
#define ICONEDITDIALOG_H

#include <QDialog>
#include <QMessageBox>
#include <QFileDialog>
#include "../controls/imageviewer.h"

namespace Ui {
class IconEditDialog;
}

class IconEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IconEditDialog(QWidget *parent = 0);
    ~IconEditDialog();
    PHNImage &image();
    void loadIcon(const char* iconData);
    void saveIcon(char* iconData);
    void setWindowTitle(const QString &title);

protected:
    virtual void closeEvent(QCloseEvent *event);
    virtual void showEvent(QShowEvent *event);
    void updateTitle();

private slots:
    void on_image_mouseChanged(QPoint point, Qt::MouseButtons buttons);
    void on_image_imageChanged();

    void on_cancelBtn_clicked();

    void on_acceptBtn_clicked();

    void on_importBtn_clicked();

    void on_resetBtn_clicked();

private:
    Ui::IconEditDialog *ui;
    QString title;
};

#endif // ICONEDITDIALOG_H
