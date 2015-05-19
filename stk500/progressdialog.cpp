#include "progressdialog.h"
#include "ui_progressdialog.h"

ProgressDialog::ProgressDialog(QWidget *parent) :
    QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowTitleHint),
    ui(new Ui::ProgressDialog)
{
    ui->setupUi(this);
    cancelClicked = false;
}

ProgressDialog::~ProgressDialog()
{
    delete ui;
}

void ProgressDialog::closeEvent (QCloseEvent *event)
{
    event->ignore();
}

void ProgressDialog::on_pushButton_clicked()
{
    cancelClicked = true;
}

void ProgressDialog::updateProgress(double progress, QString status) {
    if (progress < 0.0) {
        ui->progressBar->setMinimum(0);
        ui->progressBar->setMaximum(0);
    } else {
        int progVal = (int) (100.0 * progress);
        if (progVal > 100) {
            progVal = 100;
        }
        ui->progressBar->setMinimum(0);
        ui->progressBar->setMaximum(100);
        ui->progressBar->setValue(progVal);
    }
    ui->label->setText(status);
}
