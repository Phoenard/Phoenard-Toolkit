#include "portselectbox.h"
#include "stk500port.h"

PortSelectBox::PortSelectBox(QWidget *parent) :
    QComboBox(parent)
{
    isMouseOver = false;
}

void PortSelectBox::paintEvent(QPaintEvent *) {
   PHNButton::drawBase(this, isMouseOver, isDropDown);
   QPainter painter(this);

   // Draw an arrow pointing down
   const int tri_s = 5;
   const int tri_x = width() - tri_s - 5;
   const int d_s = isDropDown ? 1 : 0;
   const int tri_y = height() / 2 - 2 + d_s;
   QBrush tri_b(Qt::black);
   QPainterPath tri_path;
   tri_path.moveTo(tri_x-tri_s, tri_y);
   tri_path.lineTo(tri_x+tri_s, tri_y);
   tri_path.lineTo(tri_x, tri_y+tri_s);
   tri_path.lineTo(tri_x-tri_s, tri_y);
   painter.fillPath(tri_path, tri_b);

   // Draw text of the currently selected port
   const int txt_w = tri_x-tri_s-4;
   QFontMetrics metrics(painter.font());
   QString text = metrics.elidedText(portName(), Qt::ElideRight, txt_w);
   painter.drawText(4, 2+d_s, txt_w, height()-4, Qt::AlignVCenter, text);
}

void PortSelectBox::enterEvent(QEvent *event) {
    isMouseOver = true;
    QComboBox::enterEvent(event);
}

void PortSelectBox::leaveEvent(QEvent *event) {
    isMouseOver = false;
    QComboBox::leaveEvent(event);
}

void PortSelectBox::refreshPorts() {
    int index = 0;
    QList<QString> portNames = stk500Port::getPortNames();
    for (int pi = 0; pi < portNames.count(); pi++) {
        QString name = portNames[pi];
        if (index >= count()) {
            addItem(name);
        } else if (itemText(index) != name) {
            // Added (inserted) or was an item before it removed?
            int foundIndex = -1;
            for (int i = index + 1; i < count(); i++) {
                if (itemText(i) == name) {
                    foundIndex = i;
                    break;
                }
            }
            if (foundIndex != -1) {
                // Item was found; remove all items in between
                while (index < foundIndex) {
                    removeItem(index);
                }
            } else {
                // Insert the item
                insertItem(index, name);
            }
        }
        index++;
    }

    // Remove items past the ports listing
    while (index < count()) {
        removeItem(index);
    }
}

QString PortSelectBox::portName() {
    int idx = currentIndex();
    return (idx == -1) ? "" : itemText(idx);
}

void PortSelectBox::showPopup() {
    refreshPorts();
    isDropDown = true;
    update();
    QComboBox::showPopup();
}

void PortSelectBox::hidePopup() {
  QComboBox::hidePopup();
  isDropDown = false;
}
