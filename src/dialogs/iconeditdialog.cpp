#include "iconeditdialog.h"
#include "ui_iconeditdialog.h"
#include <QDebug>

IconEditDialog::IconEditDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::IconEditDialog)
{
    ui->setupUi(this);

    Qt::WindowFlags flags = this->windowFlags();
    flags |= Qt::WindowMaximizeButtonHint;
    flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags( flags );
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

void IconEditDialog::on_image_mouseChanged(int x, int y, Qt::MouseButtons buttons) {
    if (buttons & Qt::LeftButton) {
        ui->image->setPixel(x, y, QColor(Qt::black));
    }
    if (buttons & Qt::RightButton) {
        ui->image->setPixel(x, y, QColor(Qt::white));
    }
}

void IconEditDialog::on_image_imageChanged() {
    ui->preview->setPixmap(ui->image->previewImage());
}
