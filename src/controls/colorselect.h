#ifndef COLORSELECT_H
#define COLORSELECT_H

#include <QWidget>
#include <QListWidget>
#include "imageviewer.h"

class ColorSelect : public QWidget
{
    Q_OBJECT
public:
    explicit ColorSelect(QWidget *parent = 0);
    void setColorMode(ImageFormat format);
    void setColorMode(bool trueColor, int colorCount);
    void setColor(int index, QColor &color);
signals:

public slots:

private:
    QListWidget *list;
    bool trueColor;
    int colorCount;
};

#endif // COLORSELECT_H
