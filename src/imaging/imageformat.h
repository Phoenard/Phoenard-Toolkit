#ifndef IMAGEFORMAT_H
#define IMAGEFORMAT_H

#include <QString>

enum ImageFormat { LCD1, LCD2, LCD4, LCD8, LCD16, BMP8, BMP24, INVALID };

QString getFormatName(ImageFormat format);
int getFormatColors(ImageFormat format);
bool isLCDFormat(ImageFormat format);
bool isFullColorFormat(ImageFormat format);

#endif // IMAGEFORMAT_H
