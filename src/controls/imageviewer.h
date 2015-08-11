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
    void setSelection(QPainterPath path);
    QPainterPath &selection() { return selectionPath; }

protected:
    void onMouseChanged(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private:
    void updatePath(QPainterPath path);

signals:
    void mouseChanged(QPoint position, Qt::MouseButtons buttons);
    void imageChanged();

private slots:
    void on_image_changed();
    void on_selection_update();

private:
    PHNImage _image;
    QRect drawnImageBounds;
    qreal drawnImageScale;
    QPoint lastMousePos;
    QPoint pressStartPos;
    QPainterPath selectionPath;
    int dashOffset;
};

#endif // IMAGEVIEWER_H
