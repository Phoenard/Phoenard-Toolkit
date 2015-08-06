#ifndef IMAGEEDITOR_H
#define IMAGEEDITOR_H

#include <QWidget>
#include <QMouseEvent>
#include "quantize.h"

enum ImageFormat { LCD1, LCD2, LCD4, LCD8, LCD16, BMP8, BMP24, INVALID };

QString getFormatName(ImageFormat format);
int getFormatColors(ImageFormat format);
bool isLCDColor(ImageFormat format);
bool isTrueColor(ImageFormat format);
QColor color565_to_rgb(quint16 color565);
quint16 rgb_to_color565(QColor &color);

class ImageEditor : public QWidget
{
    Q_OBJECT
public:
    explicit ImageEditor(QWidget *parent = 0);
    void loadImage(QString filePath);
    void loadImage(QByteArray &data);
    void loadImageRaw(int width, int height, int bpp, QByteArray &data);
    void loadImageRaw(int width, int height, int bpp, QByteArray &data, QList<QColor> &pixelmap);
    QByteArray saveImage();
    void saveImageTo(QString destFilePath);
    const ImageFormat imageFormat() { return sourceImageFormat; }
    const ImageFormat outputImageFormat() { return destImageFormat; }
    int imageFormatColors();
    const bool imageValid() { return sourceImageValid; }
    const QSize imageSize() { return sourceImage.size(); }
    const bool outputImageTrueColor() { return isTrueColor(destImageFormat); }
    void setFormat(ImageFormat format, int colorCount = -1);
    void setHeader(bool header) { this->destImageHeader = header; }
    bool hasHeader() { return this->destImageHeader; }
    QColor getColor(int index);
    const int getColorCount() { return quant.colors; }
    void setColor(int index, QColor color);
    void setPixel(int x, int y, QColor color);
    QColor pixel(int x, int y);
    void fill(QColor color);
    void paintEvent(QPaintEvent *e);
    QPixmap &previewImage() { return preview; }

protected:
    void onMouseChanged(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

signals:
    void mouseChanged(int x, int y, Qt::MouseButtons buttons);
    void imageChanged();

private:
    QImage sourceImage;
    bool sourceImageValid;
    ImageFormat sourceImageFormat;
    int destImageColors;
    ImageFormat destImageFormat;
    bool destImageHeader;
    Quantize::Cube quant;
    QPixmap preview;
    QRect drawnImageBounds;
    QPoint lastMousePos;
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

#endif // IMAGEEDITOR_H
