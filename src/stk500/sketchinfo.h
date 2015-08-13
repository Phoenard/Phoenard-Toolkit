#ifndef SKETCHINFO_H
#define SKETCHINFO_H

#include <QIcon>
#include <QMetaType>

typedef struct SketchInfo {
    QString name;
    quint32 iconBlock;
    char iconData[512];
    QIcon icon;
    bool hasIcon;
    bool iconDirty;
    int index;

    void setIcon(const char* iconData) {
        /* Copy raw icon data */
        memcpy(this->iconData, iconData, sizeof(this->iconData));

        /* Define the two colors used to display the icon */
        QRgb color0 = QColor(0, 0, 0).rgb();
        QRgb color1 = QColor(255, 255, 255).rgb();

        /* Generate 64x64 Image from 1-bit image data */
        QImage iconImage(64, 64, QImage::Format_ARGB32);
        int x = 0, y = 0;
        unsigned char pix_dat = 0;
        for (int pix = 0; pix < (64*64); pix++) {

            /* Refresh pixel data every 8 pixels */
            if ((pix & 0x7) == 0) pix_dat = *iconData++;

            /* Push the pixel */
            iconImage.setPixel(x, y, (pix_dat & 0x1) ? color1 : color0);

            /* Next pixel data bit */
            pix_dat >>= 1;

            /* Increment current pixel */
            x++;
            if (x >= 64) {
                x = 0;
                y++;
            }
        }

        /* Generate QIcon from the image */
        this->icon = QIcon(QPixmap::fromImage(iconImage));
    }
} SketchInfo;

Q_DECLARE_METATYPE(SketchInfo)

#endif // SKETCHINFO_H
