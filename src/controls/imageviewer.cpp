#include "imageviewer.h"
#include <QFile>
#include <QPainter>
#include <QDebug>
#include <QTime>
#include <QtEndian>

ImageViewer::ImageViewer(QWidget *parent) :
    QWidget(parent) {
    this->setMouseTracking(true);

    // Notify this widget of changes to the image
    connect(&_image, SIGNAL(changed()), this, SLOT(on_image_changed()), Qt::QueuedConnection);
}

void ImageViewer::onMouseChanged(QMouseEvent* event) {
    QPoint pos = event->pos();
    if (!drawnImageBounds.contains(pos)) {
        lastMousePos = QPoint(-1, -1);
        return;
    }
    int x = (_image.width() * (pos.x() - drawnImageBounds.x())) / drawnImageBounds.width();
    int y = (_image.height() * (pos.y() - drawnImageBounds.y())) / drawnImageBounds.height();
    if ((event->button() == Qt::NoButton) && (x == lastMousePos.x()) && (y == lastMousePos.y())) {
        return;
    }
    lastMousePos = QPoint(x, y);
    emit mouseChanged(x, y, event->buttons());
}

void ImageViewer::on_image_changed() {
    update();
    emit imageChanged();
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event) {
    onMouseChanged(event);
}

void ImageViewer::mousePressEvent(QMouseEvent *event) {
    onMouseChanged(event);
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event) {
    onMouseChanged(event);
}

void ImageViewer::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    if (_image.isNull()) return;

    QRect dest(0, 0, _image.width(), _image.height());

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
    painter.drawPixmap(dest, _image.pixmap());
}
