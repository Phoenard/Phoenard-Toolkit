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

PHNImage &IconEditDialog::image() {
    return ui->image->image();
}

IconEditDialog::~IconEditDialog()
{
    delete ui;
}

void IconEditDialog::loadIcon(const char* iconData) {
    image().loadData(64, 64, 1, QByteArray(iconData, 512));
}

void IconEditDialog::saveIcon(char* iconData) {
    QByteArray data = image().saveImage();
    memcpy(iconData, data.data(), 512);
}

void IconEditDialog::setWindowTitle(const QString &title) {
    this->title = title;
    QDialog::setWindowTitle(title);
}

void IconEditDialog::closeEvent(QCloseEvent *event) {
    if (image().isEdited()) {
        // Ask the user if we really want to save this
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Save icon", "The icon was changed. Do you want to save?",
                                        QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel) {
            event->ignore();
        } else if (reply == QMessageBox::Yes) {
            this->accept();
        } else {
            this->reject();
        }
    } else {
        this->reject();
    }
}

void IconEditDialog::on_image_mouseChanged(QPoint point, Qt::MouseButtons buttons) {
    if (buttons & Qt::LeftButton) {
        image().setPixel(point.x(), point.y(), QColor(Qt::black));
    }
    if (buttons & Qt::RightButton) {
        image().setPixel(point.x(), point.y(), QColor(Qt::white));
    }
    QPainterPath sel;
    sel.addRect(point.x(), point.y(), 1, 1);
    ui->image->setSelection(sel);
}

void IconEditDialog::on_image_imageChanged() {
    ui->preview->setPixmap(image().pixmap());
    if (image().isEdited()) {
        QDialog::setWindowTitle(this->title + "*");
    }
}

void IconEditDialog::on_cancelBtn_clicked()
{
    this->reject();
}

void IconEditDialog::on_acceptBtn_clicked()
{
    this->accept();
}
