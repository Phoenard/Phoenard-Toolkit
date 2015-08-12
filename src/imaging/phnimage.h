#ifndef PHNIMAGE_H
#define PHNIMAGE_H

#include <QPixmap>
#include <QPainter>
#include <QFile>
#include <QDebug>
#include <QTime>
#include "quantize.h"
#include "imageformat.h"

QColor color565_to_rgb(quint16 color565);

quint16 rgb_to_color565(Quantize::Pixel &color);

QList<QPoint> getLinePoints(QPoint p0, QPoint p1);

QPoint getPointWithin(QPoint point, QRect rectangle);

class PHNImage : public QObject
{
    Q_OBJECT

public:
    PHNImage();
    QPixmap &pixmap() { return this->_pixmap; }
    int width() { return this->quant.width; }
    int height() { return this->quant.height; }
    bool isNull() { return !this->sourceImageValid; }

    void loadFile(QString filePath);
    void loadData(QByteArray &data);
    void loadData(int width, int height, int bpp, QByteArray &data);
    void loadData(int width, int height, int bpp, QByteArray &data, QList<QColor> &pixelmap);
    QByteArray saveImage();
    void saveImageTo(QString destFilePath);
    const ImageFormat imageFormat() { return sourceImageFormat; }
    const ImageFormat outputImageFormat() { return destImageFormat; }
    const bool imageValid() { return sourceImageValid; }
    const QSize imageSize() { return sourceImage.size(); }
    const bool isFullColor() { return quant.trueColor; }
    void setFormat(ImageFormat format, int colorCount = -1);
    void resize(int newWidth, int newHeight);
    void setColors(QList<QColor> &colors);
    void setHeader(bool header) { this->destImageHeader = header; }
    bool hasHeader() { return this->destImageHeader; }
    bool isModified() { return this->modified; }
    void resetModified() { this->modified = false; }
    QColor getColor(int index);
    const int getColorCount() { return quant.colors; }
    void setColor(int index, QColor color);
    void setPixel(int x, int y, QColor color);
    QColor pixel(int x, int y);
    void fill(QColor color);

protected:
    void onChanged();

signals:
    void changed();

private:
    QPixmap _pixmap;
    QImage sourceImage;
    ImageFormat sourceImageFormat;
    bool sourceImageValid;
    Quantize::Cube quant;
    ImageFormat destImageFormat;
    bool destImageHeader;
    bool modified;
};

typedef struct Imageheader_LCD {
    quint8 header[3];
    quint8 bpp;
    quint16 width;
    quint16 height;
    quint16 colors;
    // color values
    // pixel data
} Imageheader_LCD;

#endif // PHNIMAGE_H
