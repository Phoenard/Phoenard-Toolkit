#include "phnimage.h"

QColor color565_to_rgb(quint16 color565) {
    QColor rval;
    rval.setAlpha(0xFF);
    rval.setRed((color565 & 0xF800) >> 8);
    rval.setGreen((color565 & 0x07E0) >> 3);
    rval.setBlue((color565 & 0x001F) << 3);
    return rval;
}

quint16 rgb_to_color565(Quantize::Pixel &color) {
    quint16 rval;
    rval = color.color.r >> 3;
    rval <<= 6;
    rval |= color.color.g >> 2;
    rval <<= 5;
    rval |= color.color.b >> 3;
    return rval;
}

static void strip_color565(Quantize::Pixel &color) {
    // Retain last 5 bits of red
    color.color.r &= 0xF8;
    // Retain last 6 bits of green
    color.color.g &= 0xFC;
    // Retain last 5 bits of blue
    color.color.b &= 0xF8;
}

QList<QPoint> getLinePoints(QPoint p0, QPoint p1) {
    QList<QPoint> points;
    int x0 = p0.x(), y0 = p0.y();
    int x1 = p1.x(), y1 = p1.y();

    if ((x0 == x1) || (y0 == y1)) {
        // CALC  QPoint(27,3)  TO  QPoint(27,5)
        // Handle straight lines using simple signum logic
        int mx, my;
        do {
            points.append(QPoint(x0, y0));
            mx = (x1 > x0) - (x1 < x0);
            my = (y1 > y0) - (y1 < y0);
            x0 += mx; y0 += my;
        } while ((mx != 0) || (my != 0));

    } else {
        // Handle other lines using the Bresenham's algorithm
        // Taken from the Phoenard Arduino library
        int dx = (x1 - x0), dy = (y1 - y0);
        int adx = abs(dx), ady = abs(dy);
        bool steep = ady > adx;
        if (steep) {
          std::swap(x0, y0);
          std::swap(x1, y1);
          std::swap(adx, ady);
        }
        if (x0 > x1) {
          std::swap(x0, x1);
          std::swap(y0, y1);
        }

        int err = adx;
        int ystep = (y0 < y1) ? 1 : -1;

        adx += adx;
        ady += ady;
        for (; x0<=x1; x0++) {
            points.append(steep ? QPoint(y0, x0) : QPoint(x0, y0));
            err -= ady;
            if (err < 0) {
                y0 += ystep;
                err += adx;
            }
        }

        // Reverse the points in the list if required
        if (points[0] != p0) {
            for(int k = 0; k < (points.size()/2); k++) {
                points.swap(k, points.size()-(1+k));
            }
        }
    }
    return points;
}

QPoint getPointWithin(QPoint point, QRect rectangle) {
    if (rectangle.contains(point)) {
        return point;
    }
    int x = point.x(), y = point.y();
    int xa = rectangle.x(), ya = rectangle.y();
    int xb = rectangle.right(), yb = rectangle.bottom();
    if (x < xa) x = xa;
    if (y < ya) y = ya;
    if (x > xb) x = xb;
    if (y > yb) y = yb;
    return QPoint(x, y);
}

PHNImage::PHNImage()
{
    this->sourceImageValid = false;
    this->modified = false;
}

void PHNImage::onChanged() {
    modified = true;
    emit changed();
}

void PHNImage::loadFile(QString filePath) {
    QFile sourceFile(filePath);
    sourceFile.open(QIODevice::ReadOnly);
    if (sourceFile.isOpen()) {
        QByteArray data = sourceFile.readAll();
        sourceFile.close();
        loadData(data);
    } else {
        qDebug() << "IMAGE LOAD FAIL";
    }
}

void PHNImage::loadData(QByteArray &data) {
    // By default, loading fails
    sourceImageValid = false;
    sourceImageFormat = INVALID;
    destImageHeader = true;
    quant.erase();

    // Fails instantly if the length is below header size
    if (data.size() < 10) {
        quant.erase();
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
            loadData(header.width, header.height, header.bpp, pixeldata, pixelmap);
            destImageHeader = true;
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

void PHNImage::loadData(int width, int height, int bpp, QByteArray &data) {
    QList<QColor> defaultColors;
    if (bpp <= 8) {
        int colorCount = 1 << bpp;
        for (int i = 0; i < colorCount; i++) {
            float f = (float) i / (float) (colorCount - 1);
            defaultColors.append(QColor::fromRgbF(f, f, f));
        }
    }
    loadData(width, height, bpp, data, defaultColors);
}

void PHNImage::loadData(int width, int height, int bpp, QByteArray &data, QList<QColor> &pixelmap) {
    sourceImage = QImage(width, height, QImage::Format_ARGB32);
    sourceImageValid = true;
    destImageHeader = false;
    quant.erase();
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

        // Update the preview image information
        setFormat(sourceImageFormat);
    } else {
        // Load as 1/2/4/8-bit raw colormapped image
        // To preserve the colormap, make sure to load manually
        quant.erase();
        quant.trueColor = false;
        quant.width = width;
        quant.height = height;
        quant.max_colors = 1 << bpp;
        quant.colors = pixelmap.length();
        quant.colormap = new Quantize::Pixel[quant.colors];
        for (int i = 0; i < quant.colors; i++) {
            quant.colormap[i].rgb = pixelmap[i].rgb();
        }
        quant.pixels = new Quantize::Pixel*[quant.width];
        for (int i = 0; i < quant.width; i++) {
            quant.pixels[i] = new Quantize::Pixel[quant.height];
        }

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
                uint index = tmpbuff & pixelmask;
                quant.pixels[x][y].value = index;
                sourceImage.setPixel(x, y, quant.colormap[index].rgb);
                tmpbuff >>= bpp;
                tmpbuff_len -= bpp;
            }
        }

        destImageFormat = sourceImageFormat;
        _pixmap = QPixmap::fromImage(sourceImage);
        onChanged();
    }
}

void PHNImage::setFormat(ImageFormat format, int colorCount) {
    if (!sourceImageValid) {
        destImageFormat = INVALID;
        quant.erase();
        onChanged();
        return;
    }
    destImageFormat = format;
    if (colorCount == -1) {
        colorCount = getFormatColors(destImageFormat);
    }
    if (quant.colors == colorCount) {
        return;
    }

    // Time the quantification magic
    QTime myTimer;
    myTimer.start();

    // Load the image data into the quantizer
    quant.loadImage(sourceImage);

    // Preset color count
    bool strip565 = isLCDFormat(destImageFormat) && !isLCDFormat(sourceImageFormat);
    quant.max_colors = quant.colors = strip565 ? 0x10000 : 0x1000000;

    // Reduce the amount of colors into a colormap if needed
    if (!isFullColorFormat(destImageFormat)) {
        quant.reduce(colorCount);

        // For colormapped images, we need to sort the color indices
        if (isFullColorFormat(sourceImageFormat)) {
            // For source images of true color, this is done from dark to light colors
            quant.sortColors();
        } else {
            // For source colormapped images, this is done by stay close to the original indices
            //TODO
        }
    }

    // Strip non-565 colors if needed
    if (strip565) {
        if (quant.trueColor) {
            int x, y;
            for (x = 0; x < quant.width; x++) {
                for (y = 0; y < quant.height; y++) {
                    strip_color565(quant.pixels[x][y]);
                }
            }
        } else {
            for (int i = 0; i < quant.colors; i++) {
                strip_color565(quant.colormap[i]);
            }
        }
    }

    // Turn the quantized result into a pixmap for display purposes
    _pixmap = QPixmap::fromImage(quant.toImage());

    qDebug() << "Quantization to" << colorCount << "colors"
             << "; format =" << getFormatName(destImageFormat)
             << "; time =" << myTimer.elapsed() << "ms"
             << "; result color count =" << quant.colors;

    this->onChanged();
}

void PHNImage::resize(int newWidth, int newHeight) {
    if (this->isNull()) return;
    if ((quant.width == newWidth) && (quant.height == newHeight)) return;

    // Resize the source image to the new dimensions
    sourceImage = sourceImage.scaled(newWidth, newHeight);

    // Force an update by setting the format
    quant.erase();
    setFormat(destImageFormat);
}

void PHNImage::setColors(QList<QColor> &colors) {
    if (this->isNull()) return;

    // Reduce to a certain amount of colors if needed
    if (quant.trueColor || (quant.colors > colors.length())) {
        quant.loadImage(sourceImage);
        quant.reduce(colors.length());
    }

    // Resize the colormap if needed
    quant.setColormapSize(colors.length());

    // Figure out what colors to bind to which colormap channel
    // This is done by comparing the colors and matching best fit
    for (int index = 0; index < colors.length(); index++) {
        Quantize::Pixel p;
        p.rgb = colors[index].rgb();

        // Find the best fit color in the colormap
        // Start at the index last set (index of the current color)
        uint foundIndex = quant.findColor(p, index);

        // Swap indices to put this found index at the current index
        quant.swapColors(index, foundIndex);

        // Apply to colormap
        quant.colormap[index] = p;
    }

    // All done; update pixmap for display
    _pixmap = QPixmap::fromImage(quant.toImage());
    onChanged();
}

QColor PHNImage::getColor(int index) {
    return QColor(this->quant.colormap[index].value);
}

void PHNImage::setColor(int index, QColor color) {
    this->quant.colormap[index].value = (uint) color.rgb();
}

void PHNImage::setPixel(int x, int y, QColor color) {
    // First figure out the color to actually use
    // For colormapped images, this can only be a valid entry
    // For LCD16, we need to trim the color to 16-bit
    // For TrueColor we can use any color (24-bit)
    if (!this->quant.trueColor) {
        // Colormap: find index and apply
        Quantize::Pixel p;
        p.rgb = color.rgb();
        uint index = this->quant.findColor(p);
        this->quant.pixels[x][y].value = index;
        color.setRgb(this->quant.colormap[index].rgb);

    } else if (this->destImageFormat == LCD16) {
        // True color 16-bit: Trim to 565 format
        Quantize::Pixel pixel;
        pixel.rgb = color.rgb();
        strip_color565(pixel);
        color.setRgb(pixel.rgb);
        this->quant.pixels[x][y] = pixel;

    } else {
        // True color 24-bit: directly apply color
        this->quant.pixels[x][y].rgb = color.rgb();
    }

    // Update the pixmap with this pixel
    QPainter painter(&_pixmap);
    painter.setPen(color);
    painter.drawPoint(x, y);

    this->onChanged();
}

QColor PHNImage::pixel(int x, int y) {
    Quantize::Pixel &p = quant.pixels[x][y];
    if (!quant.trueColor) {
        p = quant.colormap[p.value];
    }
    return QColor(p.rgb);
}

void PHNImage::fill(QColor color) {
    for (int x = 0; x < quant.width; x++) {
        for (int y = 0; y < quant.height; y++) {
            setPixel(x, y, color);
        }
    }
}

QByteArray PHNImage::saveImage() {
    QByteArray dataArray;
    QDataStream out(&dataArray, QIODevice::WriteOnly);
    int x, y;
    if (destImageFormat == BMP24) {
        // 24-bit bitmap
        // Take over the pixels and write them out
        QImage img_24bit(quant.width, quant.height, QImage::Format_ARGB32);
        for (x = 0; x < quant.width; x++) {
            for (y = 0; y < quant.height; y++) {
                img_24bit.setPixel(x, y, quant.pixels[x][y].rgb);
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
                Quantize::Pixel &p = quant.colormap[quant.pixels[x][y].value];
                img_8bit.setPixel(x, y, p.rgb);
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
            if (destImageHeader) {
                out.writeRawData((char*) &lcd_header, sizeof(lcd_header));
            }

            // Write out the pixel data
            for (y = 0; y < lcd_header.height; y++) {
                for (x = 0; x < lcd_header.width; x++) {
                    color = rgb_to_color565(quant.pixels[x][y]);
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

            if (destImageHeader) {
                out.writeRawData((char*) &lcd_header, sizeof(lcd_header));

                // Write out pixelmap
                for (int i = 0; i < quant.colors; i++) {
                    color = rgb_to_color565(quant.colormap[i]);
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

void PHNImage::saveImageTo(QString destFilePath) {
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
