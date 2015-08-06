#ifndef ICONEDITDIALOG_H
#define ICONEDITDIALOG_H

#include <QDialog>
#include "../imageeditor.h"

namespace Ui {
class IconEditDialog;
}

class IconEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IconEditDialog(QWidget *parent = 0);
    ~IconEditDialog();
    void loadIcon(const char* iconData);
    void saveIcon(char* iconData);

private slots:
    void on_image_mouseChanged(int x, int y, Qt::MouseButtons buttons);
    void on_image_imageChanged();

private:
    Ui::IconEditDialog *ui;
};

#endif // ICONEDITDIALOG_H
