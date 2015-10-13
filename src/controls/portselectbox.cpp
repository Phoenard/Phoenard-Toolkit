#include "portselectbox.h"
#include "stk500port.h"
#include <QListView>

PortSelectBox::PortSelectBox(QWidget *parent) :
    QComboBox(parent)
{
    isMouseOver = false;
    isDropDown = false;

    discoverTimer.setParent(this);
    discoverTimer.setInterval(1000);
    connect(&discoverTimer, SIGNAL(timeout()), this, SLOT(runDiscovery()), Qt::QueuedConnection);

    setStyleSheet("QWidget {"
                  "  border: 0px solid black;"
                  "  margin: 0px 0px 0px 0px;"
                  "}");
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
   QString name = metrics.elidedText(portName(), Qt::ElideLeft, txt_w);
   painter.drawText(4, 2+d_s, txt_w, height()-4, Qt::AlignVCenter, name);
}

void PortSelectBox::enterEvent(QEvent *event) {
    isMouseOver = true;
    update();
    QComboBox::enterEvent(event);
}

void PortSelectBox::leaveEvent(QEvent *event) {
    isMouseOver = false;
    update();
    QComboBox::leaveEvent(event);
}

void PortSelectBox::mouseReleaseEvent(QMouseEvent *event) {
    if ((count() == 0) && (event->button() == Qt::LeftButton)) {
        isDropDown = false;
        update();
    }
}

void PortSelectBox::startDiscovery() {

    // Find all available broadcast addresses using the network sockets
    QList<QNetworkInterface> allInterfaces = QNetworkInterface::allInterfaces();
    QList<QNetworkInterface> interfaces;
    QList<QNetworkAddressEntry> addresses;
    for (int i = 0; i < allInterfaces.count(); i++) {
        QNetworkInterface &interface = allInterfaces[i];

        // Filter out loopback, inactive networks and non-broadcastable interfaces
        QNetworkInterface::InterfaceFlags flags = interface.flags();
        if (flags & QNetworkInterface::IsLoopBack) continue;
        if (!(flags & QNetworkInterface::IsRunning)) continue;
        if (!(flags & QNetworkInterface::CanBroadcast)) continue;

        // Obtain the broadcast address entries for the IPv4 host addresses
        QList<QNetworkAddressEntry> addrEntries = interface.addressEntries();
        for (int i = 0; i < addrEntries.count(); i++) {
            if (addrEntries[i].ip().protocol() == QAbstractSocket::IPv4Protocol) {
                interfaces.append(interface);
                addresses.append(addrEntries[i]);
                break;
            }
        }
    }

    // Open as many sockets as needed to receive packets
    int count = interfaces.count();
    while (count > discoverSockets.count()) {
        discoverSockets.append(new QUdpSocket(this));
    }
    while (count < discoverSockets.count()) {
        delete discoverSockets.takeLast();
    }

    // Create sockets and broadcast over all interfaces
    for (int i = 0; i < count; i++) {
        QUdpSocket *socket = discoverSockets[i];

        // Re-initialize socket if needed
        if (socket->state() != QAbstractSocket::UnconnectedState) {
            socket->abort();
            socket->close();
            delete socket;
            socket = new QUdpSocket(this);
            discoverSockets[i] = socket;
        }

        // Bind and initialize the socket
        socket->bind(addresses[i].ip(), 0);
        socket->setMulticastInterface(interfaces[i]);

        // Connect signals
        connect(socket, SIGNAL(readyRead()), this, SLOT(readDiscovery()));
    }

    // Start running discovery
    discoverTimer.start();
}

void PortSelectBox::endDiscovery() {

    // Remove created sockets
    for (int i = 0; i < discoverSockets.count(); i++) {
        delete discoverSockets[i];
    }
    discoverSockets.clear();

    // Stop running discovery
    discoverTimer.stop();
}

void PortSelectBox::runDiscovery() {
    unsigned char payload[11] = {0x1B, 0x00, 0x00, 0x05, 0x0E, 0x06,
                                 0x00, 0x00, 0x00, 0x00, 0x16};

    // Broadcast packet over all interfaces
    for (int i = 0; i < discoverSockets.count(); i++) {
        discoverSockets[i]->writeDatagram((char*) payload, sizeof(payload),
                                          QHostAddress::Broadcast, 7265);
    }

    // Increment ping counter on all ports and remove timed out connections
    for (int i = 0; i < netEntries.count(); i++) {
        NetPortEntry &entry = netEntries[i];
        if (!entry.custom && (++entry.pingCtr > 10)) {
            netEntries.removeAt(i--);
        }
    }

    // Update the ports
    portNames = stk500Port::getPortNames();

    // Update all the names
    updateNames();
}

void PortSelectBox::readDiscovery() {
    for (int i = 0; i < discoverSockets.count(); i++) {
        QUdpSocket *socket = discoverSockets[i];
        if (socket->hasPendingDatagrams()) {

            // Read the datagram host address, discard response for now
            QHostAddress deviceAddress;
            socket->readDatagram(NULL, 0, &deviceAddress);

            // Add it to the list if it does not already exist
            // Reset ping counter for the entry found
            QString deviceName = QString("net:") + deviceAddress.toString();
            bool foundEntry = false;
            for (int i = 0; i < netEntries.count(); i++) {
                if (netEntries[i].name == deviceName) {
                    netEntries[i].pingCtr = 0;
                    foundEntry = true;
                    break;
                }
            }
            if (!foundEntry) {
                NetPortEntry newEntry;
                newEntry.name = deviceName;
                newEntry.custom = false;
                newEntry.pingCtr = 0;
                netEntries.append(newEntry);
                updateNames();
            }
        }
    }
}

void PortSelectBox::updateNames() {

    // Generate the listing of items
    QList<QString> items;
    for (int i = 0; i < portNames.length(); i++) {
        items.append(portNames[i]);
    }
    for (int i = 0; i < netEntries.length(); i++) {
        items.append(netEntries[i].name);
    }
    if (items.isEmpty()) {
        items.append("");
    }
    int oldCount = this->count();

    // Add or insert the items
    int index = 0;
    for (int pi = 0; pi < items.count(); pi++) {
        QString name = items[pi];
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
                while (index < foundIndex--) {
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

    // Re-show drop down if needed
    if (isDropDown && (oldCount != items.count())) {
        QComboBox::showPopup();
    }
}

void PortSelectBox::refreshPorts() {
    portNames = stk500Port::getPortNames();
    updateNames();
}

QString PortSelectBox::portName() {
    int idx = currentIndex();
    return (idx == -1) ? "" : itemText(idx);
}

void PortSelectBox::showPopup() {
    startDiscovery();
    refreshPorts();
    isDropDown = true;
    update();
    QComboBox::showPopup();
}

void PortSelectBox::hidePopup() {
  QComboBox::hidePopup();
  isDropDown = false;
  endDiscovery();
}
