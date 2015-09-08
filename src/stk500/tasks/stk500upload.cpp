#include "../stk500Task.h"

void stk500Upload::run() {
    quint32 pageAddress = 0;
    quint32 programSize = (quint32) programData.length();
    char pageData[256];
    do {
        /* Update status */
        QString status("Writing program data: ");
        status += QString::number(pageAddress);
        status += " / ";
        status += QString::number(programData.length());
        setStatus(status);
        setProgress((double) pageAddress / (double) programSize);

        /* Prepare and write the page of data */
        int len = std::min(256, (int) (programSize - pageAddress));
        memcpy(pageData, programData.data() + pageAddress, len);
        memset(pageData+len, 0xFF, 256-len);
        protocol->FLASH_writePage(pageAddress, pageData, 256);

        /* Next page */
        pageAddress += 256;

    } while ((pageAddress < programSize) && !isCancelled());
}
