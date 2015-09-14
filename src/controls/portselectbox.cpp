#include "portselectbox.h"
#include "stk500port.h"

PortSelectBox::PortSelectBox(QWidget *parent) :
    QComboBox(parent)
{
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

void PortSelectBox::showPopup() {
    refreshPorts();
    QComboBox::showPopup();
}
