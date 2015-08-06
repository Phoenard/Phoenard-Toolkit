#include "iconeditdialog.h"
#include "ui_iconeditdialog.h"

IconEditDialog::IconEditDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::IconEditDialog)
{
    ui->setupUi(this);



}

IconEditDialog::~IconEditDialog()
{
    delete ui;
}

void IconEditDialog::loadIcon(const char* iconData) {
    ui->image->loadImageRaw(64, 64, 1, QByteArray(iconData, 512));
}

void IconEditDialog::saveIcon(char* iconData) {
    QByteArray data = ui->image->saveImage();
    memcpy(iconData, data.data(), 512);
}
