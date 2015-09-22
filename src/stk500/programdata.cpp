#include "programdata.h"

ProgramData::ProgramData()
{
}

void ProgramData::loadFile(const QString &fileName) {

    /* Load the contents of the file */
    QFile file(fileName);
    if (file.open(QFile::ReadOnly)) {
        load(file.readAll());
        file.close();
    }
}

void ProgramData::load(const QByteArray &data) {

    /* Evaluate whether this is Intel HEX Data or not */
    bool isBinary = data.length() < 12;
    for (unsigned char i = 0; i < 12 && !isBinary; i++) {
        if (data[i] < 0x30 || data[i] > 0x46) {
            isBinary = true;
        }
    }

    /* Load in the data into a big 256KB array */
    char fullData[256*1024];
    memset(fullData, 0xFF, sizeof(fullData));
    if (isBinary) {
        /* For binary data, simply copy it into the array directly */
        int len = std::min((int) sizeof(fullData), data.length());
        memcpy(fullData, data.data(), len);
    } else {
        /* For Intel Hex data, process all the entries */
        char data_buff[512];
        int dataIndex = 0;
        unsigned char length, recordtype;
        quint32 address = 0;
        quint8* address_bytes = (quint8*) &address;
        quint16* address_words = (quint16*) &address;
        for (;;) {

            /* Read in a new hex entry */
            unsigned char data_ctr = 0;
            char* buff = data_buff - 1;
            while (dataIndex < data.length()) {
                /* Read the next byte, handle block incrementing */
                char c = data[dataIndex++];
                if (data_ctr) {
                    data_ctr++;

                    /* Read the data until a char before the 0-F range is read */
                    if (c < '0') break;

                    /* Start of a new byte, increment and set to an initial 0x00 */
                    if (!(data_ctr & 0x1)) {
                        *(++buff) = 0x00;
                    }

                    /* Convert the character into HEX, put it into the buffer */
                    *buff <<= 4;
                    if (c & 0x40) {
                        c -= ('A' - '0') - 10;
                    }
                    *buff |= (c - '0') & 0xF;
                } else {
                    /* Set to 1 when : is received, starting the reading */
                    data_ctr = (c == ':');
                }
            }

            /* Process the HEX entry */
            length = data_buff[0];
            recordtype = data_buff[3];

            /* Validate that the data is valid using the provided CRC */
            unsigned char crc_expected = data_buff[length + 4];
            unsigned char crc_read = 0;
            for (int i = 0; i < (length+4); i++) crc_read += data_buff[i];
            crc_read = (~crc_read + 1) & 0xFF;
            if (crc_expected != crc_read) {
                continue;
            }

            /* Read the address part of data hex lines */
            if (!recordtype) {
                address_bytes[0] = data_buff[2];
                address_bytes[1] = data_buff[1];
            }

            /* Extended data segment, we ignore data_buff[5] (out of range) */
            if (recordtype == 0x2) {
                address_words[1] = ((quint16) data_buff[4] >> 4);
                continue;
            }

            /* Start Segment Address, detected but unused */
            if (recordtype == 0x3) {
                continue;
            }

            /* Extended Linear Address, found to exist sometimes */
            if (recordtype == 0x4) {
                address_bytes[2] = data_buff[5];
                /*address_bytes[3] = data_buff[4]; (out of range) */
                continue;
            }

            /* Fill the buffer with entry data */
            qint32 remaining = sizeof(fullData) - address;
            if (remaining > 0) {
                if (remaining < length) {
                    length = remaining;
                }
                memcpy(fullData + address, data_buff + 4, length);
            }

            /* End of data record */
            if (recordtype == 0x1) {
                break;
            }
        }
    }

    /* Now that the data has been loaded, detect firmware / sketch data ranges */
    const quint32 boot_start = 0x3E000;
    quint32 sketch_end = 0;
    quint32 firmware_end = boot_start;
    for (quint32 i = 0; i < firmware_end; i += 2) {
        if ((fullData[i] != (char) 0xFF) || (fullData[i+1] != (char) 0xFF)) {
            sketch_end = i+2;
        }
    }
    for (quint32 i = boot_start; i < sizeof(fullData); i += 2) {
        if ((fullData[i] != (char) 0xFF) || (fullData[i+1] != (char) 0xFF)) {
            firmware_end = i+2;
        }
    }

    /* Fill the sketch/firmware data buffers */
    _sketchData = QByteArray(fullData, sketch_end);
    _firmwareData = QByteArray(fullData+boot_start, firmware_end-boot_start);

    /* Make sure to pad the data to 256 page size */
    pageAlign(_sketchData);
    pageAlign(_firmwareData);
}

const char* ProgramData::sketchPage(quint32 address) {
    return _sketchData.data() + address;
}

const char* ProgramData::firmwarePage(quint32 address) {
    return _firmwareData.data() + address;
}

bool ProgramData::isServicePage(const char* pageData) {
    quint32 crc = ~0;
    for (int i = 0; i < 256; i++) {
        crc ^= (quint8) pageData[i];
        for (unsigned char k = 8; k; k--) {
          unsigned char m = (crc & 0x1);
          crc >>= 1;
          if (m) crc ^= 0xEDB88320;
        }
    }
    crc = ~crc;
    return (crc == 0xBBC8FBD5);
}

QString ProgramData::firmwareVersion() {
    // Generate CRC
    quint32 crc = ~0;
    for (int i = 0; i < _firmwareData.length(); i++) {
        crc ^= (quint8) _firmwareData[i];
        for (unsigned char k = 8; k; k--) {
          unsigned char m = (crc & 0x1);
          crc >>= 1;
          if (m) crc ^= 0xEDB88320;
        }
    }
    crc = ~crc;

    // Compile version token
    QString version = QString::number(crc, 16).toUpper();
    version += isServicePage(_firmwareData.data()) ? "-Y" : "-N";
    return version;
}

void ProgramData::pageAlign(QByteArray &data) {
    int size = data.size();
    int pageSize = (size + 256 - 1) & ~(256-1);
    data.reserve(pageSize);
    memset(data.data()+size, 0xFF, pageSize-size);
}
