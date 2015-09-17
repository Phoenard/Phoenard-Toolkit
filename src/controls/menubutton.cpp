#include "menubutton.h"
#include <QPainter>
#include <QPixmap>
#include <QDebug>

MenuButton::MenuButton(QWidget *parent) :
    QToolButton(parent) {

    // Set default font
    QFont font("Open Sans", 7);
    font.setPixelSize(10);
    this->setFont(font);

    // Set default menu button size
    this->setMinimumSize(48, 50);
    _isTab = false;
    _isHover = false;

    // Default color palette
    QPalette pal = this->palette();

    pal.setColor(QPalette::Inactive, QPalette::Button, QColor::fromRgb(255, 128, 128));
    pal.setColor(QPalette::Active, QPalette::Button, QColor::fromRgb(0, 150, 255));
    pal.setColor(QPalette::Active, QPalette::Highlight, QColor::fromRgb(128, 128, 128));
    pal.setColor(QPalette::Inactive, QPalette::Highlight, QColor::fromRgb(255, 255, 0));
    pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor::fromRgb(255, 255, 255));

    //pal.setColor(QPalette::Highlight, )
    this->setPalette(pal);
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
    const int edge_round_size = 10;
    const int edge_w = (2 * edge_round_size);
    int shape_height = height();
    if (!_isTab) {
        shape_height -= 3;
    } else if (!isChecked()) {
        shape_height -= 1;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath buttonShape;
    buttonShape.setFillRule(Qt::WindingFill);

    // Winding clockwise
    if (_isTab) {
        buttonShape.moveTo(0.5, shape_height+0.5);
        buttonShape.lineTo(0.5, edge_round_size+0.5);
        buttonShape.arcTo(0.5, 0.5, edge_w, edge_w, 180, -90);
        buttonShape.lineTo(width() - 1 - edge_round_size+0.5, 0.5);
        buttonShape.arcTo(width() - edge_w+0.5, 0.5, edge_w-1, edge_w, 90, -90);
        buttonShape.lineTo(width() - 0.5, shape_height+0.5);
        if (!isChecked()) {
            buttonShape.lineTo(0.5, shape_height+0.5);
        }
    } else {
        // Start bottom-left after the first curve
        buttonShape.addRoundedRect(0.5, 0.5, width()-1, shape_height,
                                   edge_round_size, edge_round_size);
    }

    if (!isEnabled()) {
        // Disabled flat button
        QColor color = palette().color(QPalette::Disabled, QPalette::Button);
        painter.fillPath(buttonShape, color);
    } else if (_isTab && isChecked()) {
        // Selected tab flat button
        QColor color = palette().color(QPalette::Base);
        painter.fillPath(buttonShape, color);
    } else {
        // Button state for top color: DOWN, HOVER and NORMAL
        QColor topColor;
        if (isDown()) {
            topColor = palette().color(QPalette::Active, QPalette::Highlight);
        } else if (_isHover) {
            topColor = palette().color(QPalette::Inactive, QPalette::Highlight);
        } else {
            topColor = palette().color(QPalette::Disabled, QPalette::Highlight);
        }

        // Checked state for bottom color: CHECKED and UNCHECKED
        QColor bottomColor;
        if (isChecked()) {
            bottomColor = palette().color(QPalette::Active, QPalette::Button);
        } else {
            bottomColor = palette().color(QPalette::Inactive, QPalette::Button);
        }
        if (isDown()) {
            bottomColor = PHNButton::getColorFact(bottomColor, 0.8);
        }

        // Draw using a button gradient
        qreal grad_mid = 0.45;
        QLinearGradient gradient(0, 0, 0, height());
        gradient.setColorAt(0, topColor);
        gradient.setColorAt(grad_mid-0.01, PHNButton::getGradientFactor(topColor, bottomColor));
        gradient.setColorAt(grad_mid+0.01, PHNButton::getGradientFactor(bottomColor, topColor));
        gradient.setColorAt(1, bottomColor);

        QBrush brush(gradient);
        brush.setStyle(Qt::LinearGradientPattern);
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
    painter.drawText(0, 22, width(), height() - 13, Qt::AlignCenter, this->text());
}
