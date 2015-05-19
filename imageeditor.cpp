#include "imageeditor.h"
#include <QFile>
#include <QPainter>
#include <QDebug>
#include <QTime>
#include <QtEndian>

QColor color565_to_rgb(quint16 color565) {
    QColor rval;
    rval.setAlpha(0xFF);
    rval.setRed((color565 & 0xF800) >> 8);
    rval.setGreen((color565 & 0x07E0) >> 3);
    rval.setBlue((color565 & 0x001F) << 3);
    return rval;
}

quint16 rgb_to_color565(QColor &color) {
    quint16 rval;
    rval = color.red() >> 3;
    rval <<= 6;
    rval |= color.green() >> 2;
    rval <<= 5;
    rval |= color.blue() >> 3;
    return rval;
}

QString getFormatName(ImageFormat format) {
    switch (format) {
    case BMP8: return "Bitmap (8bit, 256 colors)";
    case BMP24: return "Bitmap (24bit, true color)";
    case LCD1: return "LCD (1bit, 2 colors)";
    case LCD2: return "LCD (2bit, 4 colors)";
    case LCD4: return "LCD (4bit, 16 colors)";
    case LCD8: return "LCD (8bit, 256 colors)";
    case LCD16: return "LCD (16bit, true color)";
    default: return "Unknown format";
    }
}

int getFormatColors(ImageFormat format) {
    switch (format) {
    case BMP24:
        return 0x1000000;
    case BMP8:
        return 0x100;
    case LCD1:
        return 0x2;
    case LCD2:
        return 0x4;
    case LCD4:
        return 0x10;
    case LCD8:
        return 0x100;
    case LCD16:
        return 0x10000;
    default:
        return 0x1000000;
    }
}

bool isTrueColor(ImageFormat format) {
    return (format == BMP24) || (format == LCD16);
}

bool isLCDColor(ImageFormat format) {
    switch (format) {
    case LCD1:
    case LCD2:
    case LCD4:
    case LCD8:
    case LCD16:
        return true;
    default:
        return false;
    }
}

ImageEditor::ImageEditor(QWidget *parent) :
    QWidget(parent) {
}

void ImageEditor::loadImage(QString filePath) {
    QFile sourceFile(filePath);
    sourceFile.open(QIODevice::ReadOnly);
    if (sourceFile.isOpen()) {
        QByteArray data = sourceFile.readAll();
        sourceFile.close();
        loadImage(data);
    } else {
        qDebug() << "IMAGE LOAD FAIL";
    }
}

void ImageEditor::loadImage(QByteArray &data) {
    // By default, loading fails
    sourceImageValid = false;
    sourceImageFormat = INVALID;
    destImageColors = 0;

    // Fails instantly if the length is below header size
    if (data.size() < 10) {
        return;
    }

    // Identify .LCD images and load them as such
    if (data.at(0) == 'L' && data.at(1) == 'C' && data.at(2) == 'D') {
        QDataStream stream(data);

        // Read the LCD image header
        Imageheader_LCD header;
        stream.readRawData((char*) &header, sizeof(header));

        // Read the pixelmap information
        QList<QColor> pixelmap;
        quint16 color;
        for (int i = 0; i < header.colors; i++) {
            stream.readRawData((char*) &color, sizeof(quint16));
            pixelmap.append(color565_to_rgb(color));
        }

        if (header.bpp == 16) {
            // If 16-bit, use 'true' 16-bit color format
            // Generate the source image, filling pixel data with color565
            sourceImage = QImage(header.width, header.height, QImage::Format_ARGB32);
            sourceImageValid = true;
            sourceImageFormat = LCD16;
            int x, y;
            for (y = 0; y < header.height; y++) {
                for (x = 0; x < header.width; x++) {
                    stream.readRawData((char*) &color, sizeof(quint16));
                    sourceImage.setPixel(x, y, color565_to_rgb(color).rgb());
                }
            }

            // Update the preview image information
            setFormat(sourceImageFormat);
        } else {
            // Fill a buffer with all pixel data
            int datasize = (header.bpp * header.width * header.height) / 8;
            QByteArray pixeldata;
            pixeldata.resize(datasize);
            stream.readRawData(pixeldata.data(), datasize);

            // Read the remaining pixel data into a byte array for further loading
            loadImageRaw(header.width, header.height, header.bpp, pixeldata, pixelmap);
        }
    }

    // If loading failed at this point, try an internal loader
    if (!sourceImageValid) {
        sourceImageValid = sourceImage.loadFromData(data);
        if (sourceImageValid) {
            if (sourceImage.bitPlaneCount() == 8) {
                sourceImageFormat = BMP8;
            } else {
                sourceImageFormat = BMP24;
            }
        }

        // Update the preview image information
        setFormat(sourceImageFormat);
    }
}

void ImageEditor::loadImageRaw(int width, int height, int bpp, QByteArray &data) {
    QList<QColor> defaultColors;
    if (bpp <= 8) {
        int colorCount = 1 << bpp;
        for (int i = 0; i < colorCount; i++) {
            float f = (float) i / (float) (colorCount - 1);
            defaultColors.append(QColor::fromRgbF(f, f, f));
        }
    }
    loadImageRaw(width, height, bpp, data, defaultColors);
}

void ImageEditor::loadImageRaw(int width, int height, int bpp, QByteArray &data, QList<QColor> &pixelmap) {
    sourceImage = QImage(width, height, QImage::Format_ARGB32);
    sourceImageValid = true;
    destImageColors = 0;
    switch (bpp) {
    case 1:
        sourceImageFormat = LCD1;
        break;
    case 2:
        sourceImageFormat = LCD2;
        break;
    case 4:
        sourceImageFormat = LCD4;
        break;
    case 8:
        sourceImageFormat = LCD8;
        break;
    default:
        sourceImageFormat = LCD16;
        break;
    }

    if (sourceImageFormat == LCD16) {
        // Load as 16-bit 565 color format
        int dataIndex = 0;
        int x, y;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                quint16 color565 = data.at(dataIndex) | (data.at(dataIndex+1) << 8);
                sourceImage.setPixel(x, y, color565_to_rgb(color565).rgb());
                dataIndex += 2;
            }
        }
    } else {
        // Load as 1/2/4/8-bit raw colormapped image
        int tmpbuff = 0;
        int tmpbuff_len = 0;
        int pixelmask = (1 << bpp) - 1;
        int dataIndex = 0;
        int x, y;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                if (!tmpbuff_len) {
                    tmpbuff = (data.at(dataIndex++) & 0xFF);
                    tmpbuff_len = 8;
                }
                sourceImage.setPixel(x, y, pixelmap.at(tmpbuff & pixelmask).rgb());
                tmpbuff >>= bpp;
                tmpbuff_len -= bpp;
            }
        }
    }

    // Update the preview image information
    setFormat(sourceImageFormat);
}

void ImageEditor::setFormat(ImageFormat format, int colorCount) {
    if (!sourceImageValid) {
        destImageFormat = LCD1;
        update();
        return;
    }
    destImageFormat = format;
    int maxColors = getFormatColors(destImageFormat);
    if (colorCount == -1) {
        colorCount = maxColors;
    }
    if (destImageColors == colorCount) {
        return;
    }

    // Time the quantification magic
    QTime myTimer;
    myTimer.start();

    // If 16-bit LCD color is used, convert the color data first
    bool convert565 = isLCDColor(destImageFormat) && !isLCDColor(sourceImageFormat);

    // If converting to a true color format, do not perform quantization
    // Only perform it if the amount was clamped
    if (isTrueColor(destImageFormat) && colorCount >= maxColors) {
        // Store as true color
        quant.loadTrue(sourceImage, convert565);
    } else {
        // Perform the quantization as needed
        quant.load(sourceImage, colorCount, convert565);
    }
    destImageColors = quant.colors;

    qDebug() << "Quantization to" << colorCount << "colors"
             << "; format =" << getFormatName(destImageFormat)
             << "; time =" << myTimer.elapsed() << "ms"
             << "; result color count =" << quant.colors;

    preview = QPixmap::fromImage(quant.output);

    update();
}

int ImageEditor::imageFormatColors() {
    return destImageColors;
}

QColor ImageEditor::getColor(int index) {
    return QColor(this->quant.colormap[index].value);
}

void ImageEditor::setColor(int index, QColor color) {
    this->quant.colormap[index].value = (uint) color.rgb();
}

QByteArray ImageEditor::saveImage(bool saveHeader) {
    QByteArray dataArray;
    QDataStream out(&dataArray, QIODevice::WriteOnly);
    int x, y;
    if (destImageFormat == BMP24) {
        // 24-bit bitmap
        // Take over the pixels and write them out
        QImage img_24bit(quant.width, quant.height, QImage::Format_ARGB32);
        for (x = 0; x < quant.width; x++) {
            for (y = 0; y < quant.height; y++) {
                img_24bit.setPixel(x, y, quant.pixel(x, y).value);
            }
        }
        img_24bit.save(out.device(), "BMP");
    } else if (destImageFormat == BMP8) {
        // 8-bit bitmap
        // Note that special saving routine is needed
        // Internal 8-bit conversion is lacking
        QImage img_8bit(quant.width, quant.height, QImage::Format_Indexed8);
        img_8bit.setColorCount(quant.colors);
        for (int i = 0; i < quant.colors; i++) {
            img_8bit.setColor(i, quant.colormap[i].value);
        }
        for (x = 0; x < quant.width; x++) {
            for (y = 0; y < quant.height; y++) {
                img_8bit.setPixel(x, y, quant.pixels[x][y].value);
            }
        }
        img_8bit.save(out.device(), "BMP");
    } else {
        // lcd images
        Imageheader_LCD lcd_header;
        lcd_header.header[0] = 'L';
        lcd_header.header[1] = 'C';
        lcd_header.header[2] = 'D';
        lcd_header.width = quant.width;
        lcd_header.height = quant.height;

        // 16-bit true color LCD images
        quint16 color;
        if (destImageFormat == LCD16) {
            // 16-bit true color LCD images
            lcd_header.bpp = 16;
            lcd_header.colors = 0;

            // Write out the header
            if (saveHeader) {
                out.writeRawData((char*) &lcd_header, sizeof(lcd_header));
            }

            // Write out the pixel data
            for (y = 0; y < lcd_header.height; y++) {
                for (x = 0; x < lcd_header.width; x++) {
                    color = rgb_to_color565(QColor(quant.pixel(x, y).value));
                    out.writeRawData((char*) &color, sizeof(color));
                }
            }
        } else {
            // Figure out the bits per pixel for the formats
            switch (destImageFormat) {
            case LCD1:
                lcd_header.bpp = 1; break;
            case LCD2:
                lcd_header.bpp = 2; break;
            case LCD4:
                lcd_header.bpp = 4; break;
            default:
                lcd_header.bpp = 8; break;
            }

            // Update colors and write out the header
            // All below code relies on a properly updated quantized object!
            lcd_header.colors = quant.colors;

            if (saveHeader) {
                out.writeRawData((char*) &lcd_header, sizeof(lcd_header));

                // Write out pixelmap
                for (int i = 0; i < quant.colors; i++) {
                    color = rgb_to_color565(QColor(quant.colormap[i].value));
                    out.writeRawData((char*) &color, sizeof(color));
                }
            }

            // Write out the pixel data
            quint8 mask = (1 << lcd_header.bpp) - 1;
            quint8 data = 0;
            quint8 dataLen = 0;
            for (y = 0; y < lcd_header.height; y++) {
                for (x = 0; x < lcd_header.width; x++) {
                    data |= (quant.pixels[x][y].value & mask) << dataLen;
                    dataLen += lcd_header.bpp;
                    if (dataLen == 8) {
                        out.writeRawData((char*) &data, sizeof(data));
                        dataLen = 0;
                        data = 0;
                    }
                }
            }
            // If data remains buffered, write out an additional byte
            if (dataLen) {
                out.writeRawData((char*) &data, sizeof(data));
            }
        }
    }
    qDebug() << "Saved data length:" << dataArray.length() << "bytes";
    return dataArray;
}

void ImageEditor::saveImageTo(QString destFilePath) {
    QByteArray data = saveImage();
    QFile file(destFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data.data(), data.length());
        file.close();
        qDebug() << "File saved.";
    } else {
        qDebug() << "FAILED TO SAVE FILE";
    }
}

void ImageEditor::paintEvent(QPaintEvent *e) {
    QPainter painter(this);
    if (!sourceImageValid) return;

    QRect dest(0, 0, preview.width(), preview.height());

    // Show a scaled version of the image
    float scale_w = (float) width() / (float) dest.width();
    float scale_h = (float) height() / (float) dest.height();
    float scale = (scale_w > scale_h) ? scale_h : scale_w;
    dest.setWidth(dest.width() * scale);
    dest.setHeight(dest.height() * scale);
    dest.moveLeft((width() - dest.width()) / 2);
    dest.moveTop((height() - dest.height()) / 2);

    // Draw the actual image
    drawnImageBounds = dest;
    painter.drawPixmap(dest, preview);
}
