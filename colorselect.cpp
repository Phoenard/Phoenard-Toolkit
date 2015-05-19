#include "colorselect.h"
#include <QVBoxLayout>
#include <qDebug>

ColorSelect::ColorSelect(QWidget *parent) :
    QWidget(parent)
{
    // Default state
    this->trueColor = true;
    this->colorCount = 0;

    // Set widget to use the vertical box layout
    setLayout(new QVBoxLayout());
    layout()->setContentsMargins(0, 0, 0, 0);

    // Set up the list widget for displaying colors
    list = new QListWidget();

    // Set up the box layout widget
    setColorMode(false, 4);
}

void ColorSelect::setColorMode(ImageFormat format) {
    setColorMode(isTrueColor(format), getFormatColors(format));
}

void ColorSelect::setColorMode(bool trueColor, int colorCount) {
    if (this->trueColor != trueColor) {
        this->trueColor = trueColor;
        this->colorCount = 0;

        // Remove all widgets
        list->setVisible(false);
        layout()->removeWidget(list);

        // Add the widgets for a particular mode
        if (trueColor) {
            // Add a true color RGB selector dialog

        } else {
            // Add a colormap-based selection list
            layout()->addWidget(list);
            list->setVisible(true);
        }
    }
    if (this->colorCount != colorCount) {
        this->colorCount = colorCount;

        if (!this->trueColor) {
            // Remove items from the list if exceeding
            while (list->count() > colorCount) {
                list->takeItem(list->count() - 1);
            }

            // Add items to the list to match the requested color count
            // Resets colors and adds any required items
            for (int i = 0; i < colorCount; i++) {
                setColor(i, QColor(0xFF, 0xFF, 0xFF));
            }

        }
    }
}

void ColorSelect::setColor(int index, QColor &color) {
    if (index < 0) return;
    if (index == list->count()) {
        list->addItem("[" + QString::number(index) + "]");
    } else if (index > list->count()) {
        // Recursively set colors before this one to fill empty space
        // Should technically never be used; is a safety mechanism
        setColor(index - 1, QColor(0xFF, 0xFF, 0xFF));
    }
    // Set up the item color
    QListWidgetItem *item = list->item(index);
    item->setBackgroundColor(color);

    // Find the text color to use with the best contrast
    if (color.lightnessF() > 0.5F) {
        item->setTextColor(QColor(0x00, 0x00, 0x00));
    } else {
        item->setTextColor(QColor(0xFF, 0xFF, 0xFF));
    }
}
