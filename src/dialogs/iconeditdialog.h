#ifndef ICONEDITDIALOG_H
#define ICONEDITDIALOG_H

#include <QDialog>
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

private slots:
    void on_image_mouseChanged(QPoint point, Qt::MouseButtons buttons);
    void on_image_imageChanged();

private:
    Ui::IconEditDialog *ui;
};

#endif // ICONEDITDIALOG_H
