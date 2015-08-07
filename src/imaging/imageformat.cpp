#include "imageformat.h"

QString getFormatName(ImageFormat format) {
    switch (format) {
    case BMP8: return "Bitmap (8-bit, 256 colors)";
    case BMP24: return "Bitmap (24-bit, true color)";
    case LCD1: return "LCD (1-bit, 2 colors)";
    case LCD2: return "LCD (2-bit, 4 colors)";
    case LCD4: return "LCD (4-bit, 16 colors)";
    case LCD8: return "LCD (8-bit, 256 colors)";
    case LCD16: return "LCD (16-bit, true color)";
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

bool isFullColorFormat(ImageFormat format) {
    return (format == BMP24) || (format == LCD16);
}

bool isLCDFormat(ImageFormat format) {
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
