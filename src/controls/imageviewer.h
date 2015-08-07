#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QWidget>
#include <QMouseEvent>
#include "../imaging/phnimage.h"

class ImageViewer : public QWidget
{
    Q_OBJECT
public:
    explicit ImageViewer(QWidget *parent = 0);
    void paintEvent(QPaintEvent *e);
    PHNImage &image() { return _image; }

protected:
    void onMouseChanged(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

signals:
    void mouseChanged(int x, int y, Qt::MouseButtons buttons);
    void imageChanged();

private slots:
    void on_image_changed();

private:
    PHNImage _image;
    QRect drawnImageBounds;
    QPoint lastMousePos;
};

#endif // IMAGEVIEWER_H
