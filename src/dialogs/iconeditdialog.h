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

private:
    Ui::IconEditDialog *ui;
};

#endif // ICONEDITDIALOG_H
