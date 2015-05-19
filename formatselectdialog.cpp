#include "formatselectdialog.h"
#include "ui_formatselectdialog.h"
#include <QDebug>

int bitModes[] = {24, 1, 2, 4, 8, 16};

FormatSelectDialog::FormatSelectDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FormatSelectDialog)
{
    ui->setupUi(this);

    this->data = NULL;
    this->editor = NULL;
}

FormatSelectDialog::~FormatSelectDialog()
{
    delete ui;
}

void FormatSelectDialog::setData(ImageEditor *editor, QByteArray &data) {
    this->data = &data;
    this->editor = editor;
    int size = data.size();

    // Attempt to load for the first mode
    editor->loadImage(data);
    ImageFormat format = editor->imageFormat();
    QSize imageSize;
    if (editor->imageValid()) imageSize = editor->imageSize();

    // Add single option for detected format
    ui->fmtBox->addItem(getFormatName(format));

    // Fill options based on size
    long totalBits = (long) size * 8;
    for (int i = 1; i < 6; i++) {
        int bpp = bitModes[i];

        QString bitText = "RAW LCD " + QString::number(bpp) + "-bits";
        ui->fmtBox->addItem(bitText);

        if (totalBits % bpp) {
            continue;
        }

        long totalPixels = totalBits / bpp;
        int mid = sqrt(totalPixels);
        for (int k = mid; k >= 1; k--) {
            if (totalPixels % k) {
                continue;
            }

            // Add the found options
            int h = k;
            int w = totalPixels / k;

            // Never add resolutions exceeding in ratio by 256
            if (w > (h * 256)) {
                continue;
            }

            resolutions[i].append(QSize(w, h));
            if (w != h) {
                resolutions[i].append(QSize(h, w));
            }
        }
    }

    if (format == INVALID) {
        ui->fmtBox->setCurrentIndex(1);
    } else {
        resolutions[0].append(imageSize);
        ui->fmtBox->setCurrentIndex(0);
    }

    on_fmtBox_activated(ui->fmtBox->currentIndex());
}

void FormatSelectDialog::on_fmtBox_activated(int index)
{
    ui->resBox->clear();
    if (index == 0) {
        editor->loadImage(*data);
    } else if (index == -1) {
        return;
    }
    for (int i = 0; i < resolutions[index].size(); i++) {
        QSize res = resolutions[index].at(i);
        QString text = QString::number(res.width()) + " x " + QString::number(res.height());
        ui->resBox->addItem(text);
    }
    if (ui->resBox->count() > 0) {
        ui->resBox->setCurrentIndex(0);
    }
}

void FormatSelectDialog::on_resBox_currentIndexChanged(int index)
{
    int fmtIndex = ui->fmtBox->currentIndex();
    if (index == -1 || fmtIndex <= 0) {
        return;
    }

    QSize res = resolutions[fmtIndex].at(index);
    editor->loadImageRaw(res.width(),res.height(), bitModes[fmtIndex], *data);
}
