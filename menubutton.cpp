#include "menubutton.h"
#include <QPainter>
#include <QPixmap>
#include <QDebug>

MenuButton::MenuButton(QWidget *parent) :
    QToolButton(parent) {

    QFont font = this->font();
    font.setPointSize(7);
    this->setFont(font);

    this->setMinimumSize(48, 50);
    _isTab = false;
    _isHover = false;
}

void MenuButton::setIsTab(bool isTab) {
    _isTab = isTab;
    update();
}

void MenuButton::enterEvent(QEvent *event)
{
    this->QToolButton::enterEvent(event);
    _isHover = true;
    update();
}

void MenuButton::leaveEvent(QEvent *event)
{
    this->QToolButton::leaveEvent(event);
    _isHover = false;
    update();
}

void MenuButton::paintEvent(QPaintEvent *) {
    const int edge_round_size = 14;
    const int edge_w = (2 * edge_round_size);
    int shape_height = height();
    if (!_isTab) {
        shape_height -= 3;
    } else if (!isChecked()) {
        shape_height -= 1;
    }

    QPainter painter(this);
    QPainterPath buttonShape;
    buttonShape.setFillRule(Qt::WindingFill);

    // Winding clockwise
    if (_isTab) {
        buttonShape.moveTo(0, shape_height);
        buttonShape.lineTo(0, edge_round_size);
        buttonShape.arcTo(0, 0, edge_w, edge_w, 180, -90);
        buttonShape.lineTo(width() - 1 - edge_round_size, 0);
        buttonShape.arcTo(width() - edge_w, 0, edge_w, edge_w, 90, -90);
        buttonShape.lineTo(width() - 1, shape_height);
        if (!isChecked()) {
            buttonShape.lineTo(0, shape_height);
        }
    } else {
        // Start bottom-left after the first curve
        buttonShape.moveTo(0, shape_height - edge_round_size);
        buttonShape.lineTo(0, edge_round_size);
        buttonShape.arcTo(0, 0, edge_w, edge_w, 180, -90);
        buttonShape.lineTo(width() - 1 - edge_round_size, 0);
        buttonShape.arcTo(width() - edge_w, 0, edge_w, edge_w, 90, -90);
        buttonShape.lineTo(width() - 1, shape_height - edge_round_size);
        buttonShape.arcTo(width() - edge_w, shape_height - edge_w, edge_w, edge_w, 0, -90);
        buttonShape.lineTo(edge_round_size + 1, shape_height - 1);
        buttonShape.arcTo(0, shape_height - edge_w, edge_w, edge_w, 270, -90);
    }

    if (!isEnabled()) {
        // Disabled flat button
        QColor color = palette().color(QPalette::Disabled, QPalette::Button);
        painter.fillPath(buttonShape, color);
    } else if (_isTab && isChecked()) {
        // Selected tab flat button
        QColor color = QColor::fromRgb(255, 255, 255);
        painter.fillPath(buttonShape, color);
    } else {
        QColor topColor;

        // Button state for top color: DOWN, HOVER and NORMAL
        if (isDown()) {
            topColor = QColor::fromRgb(128, 128, 128);
        } else if (_isHover) {
            if (isChecked()) {
                topColor = QColor::fromRgb(128, 255, 255);
            } else {
                topColor = QColor::fromRgb(255, 255, 0);
            }
        } else {
            topColor = QColor::fromRgb(255, 255, 255);
        }

        // Checked state for bottom color: CHECKED and UNCHECKED
        QColor bottomColor;
        if (isChecked()) {
            bottomColor = QColor::fromRgb(0, 150, 255);
        } else {
            bottomColor = QColor::fromRgb(255, 128, 128);
        }

        // Reversed gradient when pressed down
        QLinearGradient gradient(0, 0, 0, 35);
        gradient.setColorAt(0, topColor);
        gradient.setColorAt(1, bottomColor);

        QBrush brush(gradient);
        brush.setStyle(Qt::BrushStyle::LinearGradientPattern);
        painter.fillPath(buttonShape, brush);
    }

    painter.drawPath(buttonShape);

    // Draw icon + text
    if (!icon().isNull()) {
        QPixmap pixmap = icon().pixmap(QSize(32, 32),
                                       isEnabled() ? QIcon::Normal
                                                   : QIcon::Disabled,
                                       isChecked() ? QIcon::On
                                                   : QIcon::Off);

        painter.drawPixmap((this->width() - pixmap.width()) / 2, 4, pixmap);
    }
    painter.drawText(0, 23, width(), height() - 13, Qt::AlignCenter, this->text());
}
