#include "imageviewer.h"
#include <QFile>
#include <QPainter>
#include <QDebug>
#include <QTime>
#include <QtEndian>
#include <QPen>
#include <QColor>
#include <QTimer>

ImageViewer::ImageViewer(QWidget *parent) :
    QWidget(parent) {
    this->setMouseTracking(true);

    // Notify this widget of changes to the image
    connect(&_image, SIGNAL(changed()), this, SLOT(on_image_changed()), Qt::QueuedConnection);

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(on_selection_update()));
    timer->start(200);
    dashOffset = 0;
    lastMousePos = QPoint(-1, -1);
    pressStartPos = QPoint(-1, -1);
}

void ImageViewer::setSelection(QPainterPath path) {
    // Combine previous and new paths
    QPainterPath combined;
    combined.addPath(this->selectionPath);
    combined.addPath(path);

    // Set new path
    this->selectionPath = path;
    this->selectionPath.setFillRule(Qt::WindingFill);

    // Use bounding of combined paths to update display
    updatePath(combined);
}

void ImageViewer::updatePath(QPainterPath path) {
    QRectF boundsRel = path.boundingRect();
    qreal x = (boundsRel.x() - 1) * drawnImageScale + drawnImageBounds.x();
    qreal y = (boundsRel.y() - 1) * drawnImageScale + drawnImageBounds.y();
    qreal w = (boundsRel.width() + 2) * drawnImageScale;
    qreal h = (boundsRel.height() + 2) * drawnImageScale;
    this->update(QRect(x, y, w, h));
}

void ImageViewer::on_selection_update() {
    updatePath(this->selectionPath);
}

void ImageViewer::onMouseChanged(QMouseEvent* event) {
    QPoint pos = event->pos();
    int x = (pos.x() - drawnImageBounds.x()) / this->drawnImageScale;
    int y = (pos.y() - drawnImageBounds.y()) / this->drawnImageScale;
    bool isMouseChange = (event->button() != Qt::NoButton);
    bool shiftPressed = event->modifiers().testFlag(Qt::ShiftModifier);
    QPoint newPos(x, y);

    // While holding SHIFT, use a block alignment algorithm
    if (shiftPressed && (event->buttons() != Qt::NoButton)) {
        if (this->pressStartPos == QPoint(-1, -1)) {
            this->pressStartPos = newPos;
        }

        QPoint diff = newPos - pressStartPos;
        if (abs(diff.x()) < abs(diff.y())) {
            newPos.setX(pressStartPos.x());
        } else {
            newPos.setY(pressStartPos.y());
        }
    } else {
        this->pressStartPos = QPoint(-1, -1);
    }

    // Ignore when no movement or events happened
    if (!isMouseChange && (newPos == lastMousePos)) {
        return;
    }

    // Update position and allow boundary pixels to be registered
    // When mouse moves out of bounds, the coordinate is clipped
    // While moving outside the area, nothing happens
    QPoint p0 = lastMousePos;
    QPoint p1 = newPos;
    lastMousePos = newPos;

    QRect bounds(0, 0, _image.width(), _image.height());
    bool hasP0 = bounds.contains(p0);
    bool hasP1 = bounds.contains(p1);
    if (!hasP0) p0 = getPointWithin(p0, bounds);
    if (!hasP1) p1 = getPointWithin(p1, bounds);

    if (hasP0 || hasP1) {
        // Obtain all points in between old and new positions
        // If no previous point was known, only take new point
        QList<QPoint> points;
        points = getLinePoints(p0, p1);

        // Remove the first point (event fired before)
        if (lastMousePos != newPos) {
          points.removeAt(0);
        }

        // Go by all points and fire the event
        for (int i = 0; i < points.count(); i++) {
            emit mouseChanged(points[i], event->buttons());
        }
    }
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
    drawnImageScale = (scale_w > scale_h) ? scale_h : scale_w;
    dest.setWidth(dest.width() * drawnImageScale);
    dest.setHeight(dest.height() * drawnImageScale);
    dest.moveLeft((width() - dest.width()) / 2);
    dest.moveTop((height() - dest.height()) / 2);

    // Store and apply the pixel area transformation rectangle
    drawnImageBounds = dest;
    painter.translate(dest.x(), dest.y());
    painter.scale(drawnImageScale, drawnImageScale);

    // Draw the background image
    painter.drawPixmap(0,0,_image.pixmap());

    // Draw translucent overlay
    painter.fillPath(selectionPath, QBrush(QColor(0, 0, 255, 128)));

    // Draw the selection rectangle inside this area
    QPen p(Qt::white);
    p.setWidthF(2 / drawnImageScale);
    p.setJoinStyle(Qt::MiterJoin);
    p.setCapStyle(Qt::SquareCap);
    p.setDashPattern( QVector<qreal>() << 1 << 1.5);
    p.setDashOffset(dashOffset++);

    painter.setCompositionMode(QPainter::RasterOp_SourceXorDestination);
    painter.setPen(p);
    painter.drawPath(selectionPath);
}
