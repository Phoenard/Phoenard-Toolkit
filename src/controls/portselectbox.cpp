#include "portselectbox.h"

PortSelectBox::PortSelectBox(QWidget *parent) :
    QComboBox(parent)
{
}

void PortSelectBox::refreshPorts() {
    int index = 0;
    for (QSerialPortInfo info : QSerialPortInfo::availablePorts()) {
        QString name = info.portName();
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

void PortSelectBox::showPopup() {
    refreshPorts();
    QComboBox::showPopup();
}
