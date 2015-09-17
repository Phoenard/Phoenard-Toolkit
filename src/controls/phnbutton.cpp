#include "phnbutton.h"
#include <QPainter>

PHNButton::PHNButton(QWidget *parent) :
    QPushButton(parent)
{
    isMouseOver = false;
}

void PHNButton::enterEvent(QEvent* event) {
    isMouseOver = true;
    QPushButton::enterEvent(event);
}

void PHNButton::leaveEvent(QEvent* event) {
    isMouseOver = false;
    QPushButton::leaveEvent(event);
}

void PHNButton::paintEvent(QPaintEvent *) {
    drawBase(this, isMouseOver, isDown());
    QPainter painter(this);
    int d_s = isDown() ? 1 : 0;
    painter.drawText(2, 2+d_s, width()-5, height()-5, Qt::AlignHCenter | Qt::AlignVCenter, this->text());
}

void PHNButton::drawBase(QWidget *widget, bool isHover, bool isDown) {
    QPainter painter(widget);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal rad = 2;
    QPainterPath buttonShape;
    buttonShape.setFillRule(Qt::WindingFill);
    buttonShape.addRoundedRect(0.5, 0.5, widget->width()-1, widget->height()-1, rad, rad);

    QColor face_color(210, 210, 210);
    qreal grad_step = 1.0 / (qreal) widget->height();
    qreal grad_mid = 0.5;
    if (isDown) {
        face_color = getColorFact(face_color, 0.8);
        grad_mid += grad_step;
    } else if (isHover) {
        face_color = getColorFact(face_color, 0.95, 0.95, 1.3);
    }

    const qreal fc_fact = 0.8;
    QColor fc1 = getColorFact(face_color, 1/fc_fact);
    QColor fc2 = getColorFact(face_color, fc_fact);
    QLinearGradient gradient(0, 0, 0, widget->height());
    if (isDown) {
        // Add small shadow when pressed down
        QColor bg(0, 0, 0);
        gradient.setColorAt(1.0 * grad_step, bg);
        gradient.setColorAt(2.0 * grad_step, fc1);
    } else {
        // Otherwise, no shadow
        gradient.setColorAt(0, fc1);
    }
    gradient.setColorAt(grad_mid-0.01, getGradientFactor(fc1, fc2));
    gradient.setColorAt(grad_mid+0.01, getGradientFactor(fc2, fc1));
    gradient.setColorAt(1, fc2);

    QBrush brush(gradient);
    brush.setStyle(Qt::LinearGradientPattern);

    painter.fillPath(buttonShape, brush);
    painter.drawPath(buttonShape);
}

QColor PHNButton::getColorFact(const QColor &c, qreal factor) {
    return getColorFact(c, factor, factor, factor);
}

QColor PHNButton::getColorFact(const QColor &c, qreal fr, qreal fg, qreal fb) {
    int r = std::max(std::min((int) (c.red() * fr), 255), 0);
    int g = std::max(std::min((int) (c.green() * fg), 255), 0);
    int b = std::max(std::min((int) (c.blue() * fb), 255), 0);
    return QColor(r, g, b);
}

QColor PHNButton::getGradientFactor(const QColor &ca, const QColor &cb) {
    const qreal fb = 0.25;
    const qreal fa = 1.0 - fb;
    return QColor(fa*ca.red()   + fb*cb.red(),
                  fa*ca.green() + fb*cb.green(),
                  fa*ca.blue()  + fb*cb.blue());
}
